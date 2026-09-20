#pragma once

#include <optional>

#include <QString>

namespace foldersnap
{
struct RootPath
{
    QString displayPath;
    QString identityPath;
    bool operator==(const RootPath &) const = default;
};

[[nodiscard]] RootPath normalizeRootPath(const QString &path);
[[nodiscard]] QString normalizeRelativePath(const QString &path);
[[nodiscard]] QString normalizeRelativeDisplayPath(const QString &path);
void validateIdentityPath(const QString &path);
void validateStorageId(const QString &id);
// Lexical containment only. Live traversal/cleanup must also check reparse-point ancestors.
[[nodiscard]] bool isAtOrBelow(const QString &path, const QString &parent);
[[nodiscard]] QString joinUnderRoot(const RootPath &root, const QString &relativePath);
// A literal protected subtree, evaluated after user rules so negations cannot override it.
[[nodiscard]] std::optional<QString> protectedDataSubtree(const RootPath &root,
                                                          const RootPath &dataDirectory);
} // namespace foldersnap
