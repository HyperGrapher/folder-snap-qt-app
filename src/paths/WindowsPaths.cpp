#include "WindowsPaths.h"

#include <QDir>
#include <QStringList>

#include "domain/DomainError.h"

namespace foldersnap
{
namespace
{
[[noreturn]] void invalidPath(const QString &path)
{
    throw DomainError(ErrorCode::InvalidPath, "Unsafe or unsupported Windows path: " + path);
}

void validateComponent(const QString &component)
{
    if (component.isEmpty() || component == "." || component == ".." || component.endsWith('.') ||
        component.endsWith(' '))
    {
        invalidPath(component);
    }
    for (const QChar character : component)
    {
        if (character.unicode() < 32 || QStringLiteral("<>:\"/\\|?*").contains(character))
        {
            invalidPath(component);
        }
    }
    const QString stem = component.section('.', 0, 0).trimmed().toUpper();
    if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL" || stem == "CONIN$" ||
        stem == "CONOUT$" ||
        ((stem.startsWith("COM") || stem.startsWith("LPT")) && stem.size() == 4 &&
         (QStringLiteral("123456789¹²³").contains(stem.back()))))
    {
        invalidPath(component);
    }
}

QString separators(QString path)
{
    return path.replace('\\', '/');
}
} // namespace

RootPath normalizeRootPath(const QString &path)
{
    QString absolute = separators(path);
    if (absolute.startsWith("//?/UNC/", Qt::CaseInsensitive))
    {
        absolute = "//" + absolute.mid(8);
    }
    else if (absolute.startsWith("//?/"))
    {
        absolute = absolute.mid(4);
    }
    const bool drivePath = absolute.size() >= 3 &&
                           ((absolute[0] >= 'a' && absolute[0] <= 'z') ||
                            (absolute[0] >= 'A' && absolute[0] <= 'Z')) &&
                           absolute[1] == ':' && absolute[2] == '/';
    const bool uncPath = absolute.startsWith("//") && !absolute.startsWith("///");
    if (!drivePath && !uncPath)
    {
        invalidPath(path);
    }
    // Validate before cleaning: Windows trailing-dot, device and ADS aliases must not disappear.
    const QString tail = absolute.mid(drivePath ? 3 : 2);
    const QStringList parts = tail.split('/', Qt::SkipEmptyParts);
    if (uncPath && (parts.size() < 2 || parts[0] == "." || parts[0] == ".." || parts[1] == "." ||
                    parts[1] == ".."))
    {
        invalidPath(path);
    }
    int depth = 0;
    for (qsizetype index = 0; index < parts.size(); ++index)
    {
        const QString &part = parts[index];
        if (uncPath && index < 2)
        {
            validateComponent(part);
            continue;
        }
        if (part == "..")
        {
            if (depth == 0)
            {
                invalidPath(path);
            }
            --depth;
        }
        else if (part != ".")
        {
            validateComponent(part);
            ++depth;
        }
    }
    absolute = QDir::cleanPath(absolute);
    if (drivePath)
    {
        absolute[0] = absolute[0].toUpper();
    }
    return {absolute, absolute.toLower()};
}

QString normalizeRelativePath(const QString &path)
{
    const QString relative = separators(path);
    if (relative.isEmpty())
    {
        invalidPath(path);
    }
    const QStringList components = relative.split('/');
    for (const QString &component : components)
    {
        validateComponent(component);
    }
    return relative.toLower();
}

void validateIdentityPath(const QString &path)
{
    if (path != normalizeRelativePath(path))
    {
        invalidPath(path);
    }
}

void validateStorageId(const QString &id)
{
    try
    {
        validateComponent(id);
    }
    catch (const DomainError &)
    {
        throw DomainError(ErrorCode::InvalidIdentifier, "Invalid storage identifier: " + id);
    }
}

bool isAtOrBelow(const QString &path, const QString &parent)
{
    if (parent.isEmpty())
    {
        return false;
    }
    return path.compare(parent, Qt::CaseInsensitive) == 0 ||
           path.startsWith(parent.endsWith('/') ? parent : parent + '/', Qt::CaseInsensitive);
}

QString joinUnderRoot(const RootPath &root, const QString &relativePath)
{
    const RootPath checkedRoot = normalizeRootPath(root.displayPath);
    if (checkedRoot != root)
    {
        invalidPath(root.displayPath);
    }
    validateIdentityPath(relativePath);
    const QString joined = QDir::cleanPath(root.displayPath + '/' + relativePath);
    if (!isAtOrBelow(joined, root.displayPath) ||
        joined.compare(root.displayPath, Qt::CaseInsensitive) == 0)
    {
        invalidPath(relativePath);
    }
    return joined;
}

std::optional<QString> protectedDataSubtree(const RootPath &root, const RootPath &dataDirectory)
{
    if (normalizeRootPath(root.displayPath) != root ||
        normalizeRootPath(dataDirectory.displayPath) != dataDirectory)
    {
        invalidPath(root.displayPath);
    }
    if (isAtOrBelow(root.identityPath, dataDirectory.identityPath))
    {
        throw DomainError(ErrorCode::InvalidPath,
                          "FolderSnap's data directory and its contents cannot be watched.");
    }
    if (!isAtOrBelow(dataDirectory.identityPath, root.identityPath))
    {
        return std::nullopt;
    }
    const qsizetype offset = root.identityPath.size() + (root.identityPath.endsWith('/') ? 0 : 1);
    return dataDirectory.identityPath.mid(offset);
}
} // namespace foldersnap
