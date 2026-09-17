#include "diff/ComparisonTree.h"

#include <algorithm>

#include <QHash>
#include <QSet>

#include "domain/DomainError.h"

namespace foldersnap
{
namespace
{
struct ProjectionNode
{
    ComparisonTreeRow row;
    qint64 effectiveSize{0};
};

QString parentPath(const QString &path)
{
    const qsizetype separator = path.lastIndexOf('/');
    return separator < 0 ? QString{} : path.left(separator);
}

void checkCancelled(const std::function<bool()> &cancelled)
{
    if (cancelled && cancelled())
    {
        throw DomainError(ErrorCode::Cancelled, "Export was cancelled.");
    }
}

QHash<QString, qint64> recursiveSizes(const Snapshot &snapshot,
                                      const std::function<bool()> &cancelled)
{
    QHash<QString, qint64> sizes;
    qsizetype index = 0;
    for (const SnapshotEntry &entry : snapshot.entries)
    {
        if ((index++ % 256) == 0)
        {
            checkCancelled(cancelled);
        }
        if (entry.type != EntryType::File)
        {
            continue;
        }
        QString path = entry.path;
        sizes[path] = entry.size;
        while (path.contains('/'))
        {
            path = parentPath(path);
            sizes[path] += entry.size;
        }
    }
    return sizes;
}

int naturalCompare(const QString &left, const QString &right)
{
    qsizetype leftIndex = 0;
    qsizetype rightIndex = 0;
    while (leftIndex < left.size() && rightIndex < right.size())
    {
        if (left[leftIndex].isDigit() && right[rightIndex].isDigit())
        {
            qsizetype leftEnd = leftIndex;
            qsizetype rightEnd = rightIndex;
            while (leftEnd < left.size() && left[leftEnd].isDigit())
            {
                ++leftEnd;
            }
            while (rightEnd < right.size() && right[rightEnd].isDigit())
            {
                ++rightEnd;
            }
            QString leftNumber = left.mid(leftIndex, leftEnd - leftIndex);
            QString rightNumber = right.mid(rightIndex, rightEnd - rightIndex);
            while (leftNumber.size() > 1 && leftNumber.startsWith('0'))
            {
                leftNumber.removeFirst();
            }
            while (rightNumber.size() > 1 && rightNumber.startsWith('0'))
            {
                rightNumber.removeFirst();
            }
            if (leftNumber.size() != rightNumber.size())
            {
                return leftNumber.size() < rightNumber.size() ? -1 : 1;
            }
            const int numberComparison = QString::compare(leftNumber, rightNumber);
            if (numberComparison != 0)
            {
                return numberComparison;
            }
            leftIndex = leftEnd;
            rightIndex = rightEnd;
            continue;
        }

        const QChar leftCharacter = left[leftIndex].toCaseFolded();
        const QChar rightCharacter = right[rightIndex].toCaseFolded();
        if (leftCharacter != rightCharacter)
        {
            return leftCharacter < rightCharacter ? -1 : 1;
        }
        ++leftIndex;
        ++rightIndex;
    }
    if (leftIndex != left.size() || rightIndex != right.size())
    {
        return leftIndex == left.size() ? -1 : 1;
    }
    return QString::compare(left, right);
}

bool isOrdinaryChange(ChangeKind kind)
{
    return kind == ChangeKind::Added || kind == ChangeKind::Removed || kind == ChangeKind::Modified;
}

bool isDirectory(const std::optional<SnapshotEntry> &entry)
{
    return entry && entry->type == EntryType::Directory;
}

bool isFile(const std::optional<SnapshotEntry> &entry)
{
    return entry && entry->type == EntryType::File;
}

qint64 fileBytes(const std::optional<SnapshotEntry> &entry)
{
    return isFile(entry) ? entry->size : 0;
}
} // namespace

QList<ComparisonTreeRow> buildComparisonTree(const Snapshot &before, const Snapshot &after,
                                             const DiffResult &diff,
                                             const std::function<bool()> &cancelled)
{
    const QHash<QString, qint64> beforeSizes = recursiveSizes(before, cancelled);
    const QHash<QString, qint64> afterSizes = recursiveSizes(after, cancelled);
    QHash<QString, ProjectionNode> nodes;
    QHash<QString, QList<QString>> children;

    const auto ensureNode = [&](const QString &path, bool folder)
    {
        if (nodes.contains(path))
        {
            nodes[path].row.folder = nodes[path].row.folder || folder;
            return;
        }
        ComparisonTreeRow row;
        row.path = path;
        row.name = path.section('/', -1);
        row.depth = static_cast<int>(path.count('/'));
        row.folder = folder;
        nodes.insert(path, {row, 0});
        children[parentPath(path)].append(path);
    };

    qsizetype entryIndex = 0;
    for (const DiffEntry &entry : diff.entries)
    {
        if ((entryIndex++ % 256) == 0)
        {
            checkCancelled(cancelled);
        }
        if (!isOrdinaryChange(entry.kind))
        {
            continue;
        }
        const bool folder = isDirectory(entry.before) || isDirectory(entry.after);
        ensureNode(entry.path, folder);
        ProjectionNode &node = nodes[entry.path];
        node.row.change = entry.kind;

        if (folder)
        {
            node.row.hasBeforeSize = isDirectory(entry.before) ||
                                     beforeSizes.contains(entry.path) || isFile(entry.before);
            node.row.hasAfterSize =
                isDirectory(entry.after) || afterSizes.contains(entry.path) || isFile(entry.after);
            node.row.beforeBytes =
                isFile(entry.before) ? entry.before->size : beforeSizes.value(entry.path);
            node.row.afterBytes =
                isFile(entry.after) ? entry.after->size : afterSizes.value(entry.path);
        }
        else
        {
            node.row.hasBeforeSize = isFile(entry.before);
            node.row.hasAfterSize = isFile(entry.after);
            node.row.beforeBytes = fileBytes(entry.before);
            node.row.afterBytes = fileBytes(entry.after);
        }
        node.effectiveSize = node.row.hasAfterSize ? node.row.afterBytes : node.row.beforeBytes;

        QString parent = parentPath(entry.path);
        while (!parent.isEmpty())
        {
            ensureNode(parent, true);
            ProjectionNode &parentNode = nodes[parent];
            parentNode.row.hasBeforeSize = beforeSizes.contains(parent);
            parentNode.row.hasAfterSize = afterSizes.contains(parent);
            parentNode.row.beforeBytes = beforeSizes.value(parent);
            parentNode.row.afterBytes = afterSizes.value(parent);
            parent = parentPath(parent);
        }
    }

    for (auto iterator = children.begin(); iterator != children.end(); ++iterator)
    {
        checkCancelled(cancelled);
        std::sort(iterator.value().begin(), iterator.value().end(),
                  [&nodes](const QString &leftPath, const QString &rightPath)
                  {
                      const ProjectionNode &left = nodes[leftPath];
                      const ProjectionNode &right = nodes[rightPath];
                      if (left.row.folder != right.row.folder)
                      {
                          return left.row.folder;
                      }
                      if (!left.row.folder && left.effectiveSize != right.effectiveSize)
                      {
                          return left.effectiveSize > right.effectiveSize;
                      }
                      const int nameComparison = naturalCompare(left.row.name, right.row.name);
                      return nameComparison != 0 ? nameComparison < 0 : leftPath < rightPath;
                  });
    }

    QList<ComparisonTreeRow> rows;
    const auto appendChildren = [&](const auto &self, const QString &parent) -> void
    {
        checkCancelled(cancelled);
        for (const QString &path : children.value(parent))
        {
            rows.append(nodes[path].row);
            self(self, path);
        }
    };
    appendChildren(appendChildren, QString{});
    return rows;
}
} // namespace foldersnap
