#pragma once

#include <optional>

#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace foldersnap
{
struct IgnoreMatch
{
    bool included{true};
    QString originalRule;
    int ruleLine{0};
    bool protectedByApplication{false};
};

class IgnoreMatcher final
{
  public:
    explicit IgnoreMatcher(const QStringList &rules,
                           std::optional<QString> protectedSubtree = std::nullopt);
    [[nodiscard]] IgnoreMatch testPath(const QString &relativePath, bool isDirectory) const;
    [[nodiscard]] bool canPruneDirectory(const QString &relativePath) const;
    [[nodiscard]] static QString rulesHash(const QStringList &rules);

  private:
    struct Rule
    {
        QRegularExpression expression;
        QString original;
        int line;
        bool negated;
        bool directoryOnly;
    };
    QList<Rule> m_rules;
    std::optional<QString> m_protectedSubtree;
    bool m_hasNegations{false};
};
} // namespace foldersnap
