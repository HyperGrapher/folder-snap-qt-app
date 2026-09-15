#include "IgnoreMatcher.h"

#include <utility>

#include <QCryptographicHash>

#include "domain/DomainError.h"
#include "paths/WindowsPaths.h"

namespace foldersnap
{
namespace
{
QString globExpression(const QString &pattern)
{
    QString result;
    for (qsizetype index = 0; index < pattern.size(); ++index)
    {
        const QChar character = pattern[index];
        if (character == '*')
        {
            if (index + 1 < pattern.size() && pattern[index + 1] == '*')
            {
                ++index;
                if (index + 1 < pattern.size() && pattern[index + 1] == '/')
                {
                    ++index;
                    result += "(?:[^/]+/)*";
                }
                else
                {
                    result += ".*";
                }
            }
            else
            {
                result += "[^/]*";
            }
        }
        else if (character == '?')
        {
            result += "[^/]";
        }
        else
        {
            result += QRegularExpression::escape(QString(character));
        }
    }
    return result;
}
} // namespace

IgnoreMatcher::IgnoreMatcher(const QStringList &rules, std::optional<QString> protectedSubtree)
    : m_protectedSubtree(std::move(protectedSubtree))
{
    if (m_protectedSubtree)
    {
        validateIdentityPath(*m_protectedSubtree);
    }
    for (qsizetype index = 0; index < rules.size(); ++index)
    {
        QString pattern = rules[index].trimmed().replace('\\', '/');
        if (pattern.isEmpty() || pattern.startsWith('#'))
        {
            continue;
        }
        const bool negated = pattern.startsWith('!');
        if (negated)
        {
            pattern.remove(0, 1);
        }
        const bool anchored = pattern.startsWith('/');
        if (anchored)
        {
            pattern.remove(0, 1);
        }
        const bool directoryOnly = pattern.endsWith('/');
        if (directoryOnly)
        {
            pattern.chop(1);
        }
        const int line = static_cast<int>(index + 1);
        if (pattern.trimmed().isEmpty() || pattern.contains(QChar::Null) ||
            pattern.split('/').contains("..") || pattern.split('/').contains(".") ||
            pattern.contains("//"))
        {
            throw DomainError(
                ErrorCode::InvalidRule,
                QString("Invalid exclusion rule on line %1: %2").arg(line).arg(rules[index]), line);
        }
        const QString prefix = anchored || pattern.contains('/') ? "^" : "^(?:.*/)?";
        QRegularExpression expression(prefix + globExpression(pattern) + '$',
                                      QRegularExpression::CaseInsensitiveOption);
        if (!expression.isValid())
        {
            throw DomainError(ErrorCode::InvalidRule, expression.errorString(), line);
        }
        m_rules.push_back({expression, rules[index], line, negated, directoryOnly});
        m_hasNegations = m_hasNegations || negated;
    }
}

IgnoreMatch IgnoreMatcher::testPath(const QString &relativePath, bool isDirectory) const
{
    const QString path = normalizeRelativePath(relativePath);
    if (m_protectedSubtree && isAtOrBelow(path, *m_protectedSubtree))
    {
        return {false, '/' + *m_protectedSubtree + '/', 0, true};
    }
    IgnoreMatch result;
    // Check ancestors too: a rule matching a directory also applies to its descendants.
    QStringList prefixes;
    for (qsizetype end = path.indexOf('/'); end != -1; end = path.indexOf('/', end + 1))
    {
        prefixes.push_back(path.left(end));
    }
    prefixes.push_back(path);
    for (const Rule &rule : m_rules)
    {
        for (qsizetype index = 0; index < prefixes.size(); ++index)
        {
            const bool prefixIsDirectory = index + 1 < prefixes.size() || isDirectory;
            if ((!rule.directoryOnly || prefixIsDirectory) &&
                rule.expression.match(prefixes[index]).hasMatch())
            {
                result = {rule.negated, rule.original, rule.line, false};
                break;
            }
        }
    }
    return result;
}

bool IgnoreMatcher::canPruneDirectory(const QString &relativePath) const
{
    const IgnoreMatch match = testPath(relativePath, true);
    return match.protectedByApplication || (!m_hasNegations && !match.included);
}

QString IgnoreMatcher::rulesHash(const QStringList &rules)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(rules.join('\n').toUtf8(), QCryptographicHash::Sha256).toHex());
}
} // namespace foldersnap
