#pragma once

#include <functional>
#include <optional>

#include <QString>
#include <QtTypes>

namespace foldersnap
{
struct NativeFileMetadata
{
    quint32 attributes{0};
    qint64 createdNs{0};
    qint64 modifiedNs{0};
};

enum class DirectoryEnumerationError
{
    NotFound,
    AccessDenied,
    Io
};

// Returns a Win32 path that keeps long drive and UNC paths addressable.
[[nodiscard]] QString extendedNativePath(const QString &path);
// Returns true when any component cannot be inspected safely or is a reparse point.
[[nodiscard]] bool hasReparsePointInPath(const QString &path);
[[nodiscard]] std::optional<NativeFileMetadata> readNativeFileMetadata(const QString &path);
[[nodiscard]] std::optional<DirectoryEnumerationError>
enumerateDirectoryEntries(const QString &path,
                          const std::function<void(const QString &entryPath)> &onEntry);
} // namespace foldersnap
