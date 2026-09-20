#include "platform/windows/NativeFileMetadata.h"

#include <memory>
#include <utility>

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QStringList>

#include "domain/DomainError.h"
#include "domain/Timestamp.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace foldersnap
{
QString extendedNativePath(const QString &path)
{
#ifdef Q_OS_WIN
    QString native = QDir::toNativeSeparators(path);
    if (native.startsWith(QStringLiteral("\\\\?\\")))
    {
        return native;
    }
    if (native.startsWith(QStringLiteral("\\\\")))
    {
        return QStringLiteral("\\\\?\\UNC\\") + native.mid(2);
    }
    if (native.size() >= 3 && native[1] == QChar(':') && native[2] == QChar('\\'))
    {
        return QStringLiteral("\\\\?\\") + native;
    }
    return native;
#else
    return QDir::toNativeSeparators(path);
#endif
}

bool hasReparsePointInPath(const QString &path)
{
    QString normalized = QDir::fromNativeSeparators(path);
    if (normalized.isEmpty() || !QDir::isAbsolutePath(normalized))
    {
        return true;
    }
    normalized = QDir::cleanPath(normalized);

#ifdef Q_OS_WIN
    QString current;
    QStringList components;
    if (normalized.size() >= 3 && normalized[1] == QChar(':') && normalized[2] == QChar('/'))
    {
        current = normalized.left(3);
        components = normalized.mid(3).split('/', Qt::SkipEmptyParts);
    }
    else if (normalized.startsWith("//") && !normalized.startsWith("///"))
    {
        const QStringList uncParts = normalized.mid(2).split('/', Qt::SkipEmptyParts);
        if (uncParts.size() < 2)
        {
            return true;
        }
        current = "//" + uncParts.at(0) + '/' + uncParts.at(1);
        components = uncParts.mid(2);
    }
    else
    {
        return true;
    }

    const auto inspect = [](const QString &candidate)
    {
        const auto metadata = readNativeFileMetadata(candidate);
        return !metadata || (metadata->attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    };
    if (inspect(current))
    {
        return true;
    }
    for (const QString &component : std::as_const(components))
    {
        if (!current.endsWith('/'))
        {
            current += '/';
        }
        current += component;
        if (inspect(current))
        {
            return true;
        }
    }
    return false;
#else
    QString current = "/";
    const QStringList components = normalized.mid(1).split('/', Qt::SkipEmptyParts);
    for (const QString &component : std::as_const(components))
    {
        if (!current.endsWith('/'))
        {
            current += '/';
        }
        current += component;
        const QFileInfo info(current);
        if (!info.exists() || info.isSymLink())
        {
            return true;
        }
    }
    return false;
#endif
}

std::optional<NativeFileMetadata> readNativeFileMetadata(const QString &path)
{
#ifdef Q_OS_WIN
    const QString nativePath = extendedNativePath(path);
    const HANDLE handle =
        CreateFileW(reinterpret_cast<LPCWSTR>(nativePath.utf16()), 0,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return std::nullopt;
    }

    FILE_BASIC_INFO basicInfo{};
    const BOOL read =
        GetFileInformationByHandleEx(handle, FileBasicInfo, &basicInfo, sizeof(basicInfo));
    const DWORD error = read ? ERROR_SUCCESS : GetLastError();
    CloseHandle(handle);
    if (!read)
    {
        SetLastError(error);
        return std::nullopt;
    }

    try
    {
        return NativeFileMetadata{
            static_cast<quint32>(basicInfo.FileAttributes),
            unixNanosecondsFromFileTime(static_cast<quint64>(basicInfo.CreationTime.QuadPart)),
            unixNanosecondsFromFileTime(static_cast<quint64>(basicInfo.LastWriteTime.QuadPart))};
    }
    catch (const DomainError &)
    {
        SetLastError(ERROR_INVALID_DATA);
        return std::nullopt;
    }
#else
    Q_UNUSED(path);
    return std::nullopt;
#endif
}

std::optional<DirectoryEnumerationError>
enumerateDirectoryEntries(const QString &path,
                          const std::function<void(const QString &entryPath)> &onEntry)
{
#ifdef Q_OS_WIN
    QString searchPath = extendedNativePath(path);
    if (!searchPath.endsWith(QChar('\\')))
    {
        searchPath += QChar('\\');
    }
    searchPath += QChar('*');

    WIN32_FIND_DATAW findData{};
    const HANDLE handle =
        FindFirstFileExW(reinterpret_cast<LPCWSTR>(searchPath.utf16()), FindExInfoBasic, &findData,
                         FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (handle == INVALID_HANDLE_VALUE)
    {
        switch (GetLastError())
        {
        case ERROR_FILE_NOT_FOUND:
            return std::nullopt;
        case ERROR_PATH_NOT_FOUND:
        case ERROR_INVALID_NAME:
            return DirectoryEnumerationError::NotFound;
        case ERROR_ACCESS_DENIED:
        case ERROR_SHARING_VIOLATION:
        case ERROR_LOCK_VIOLATION:
            return DirectoryEnumerationError::AccessDenied;
        default:
            return DirectoryEnumerationError::Io;
        }
    }

    const auto closeHandle = [](HANDLE value)
    {
        if (value != INVALID_HANDLE_VALUE)
        {
            FindClose(value);
        }
    };
    const std::unique_ptr<void, decltype(closeHandle)> guard(handle, closeHandle);
    while (true)
    {
        const QString name = QString::fromWCharArray(findData.cFileName);
        if (name != QStringLiteral(".") && name != QStringLiteral(".."))
        {
            onEntry(QDir(path).filePath(name));
        }
        if (FindNextFileW(handle, &findData))
        {
            continue;
        }
        switch (GetLastError())
        {
        case ERROR_NO_MORE_FILES:
            return std::nullopt;
        case ERROR_PATH_NOT_FOUND:
        case ERROR_INVALID_NAME:
            return DirectoryEnumerationError::NotFound;
        case ERROR_ACCESS_DENIED:
        case ERROR_SHARING_VIOLATION:
        case ERROR_LOCK_VIOLATION:
            return DirectoryEnumerationError::AccessDenied;
        default:
            return DirectoryEnumerationError::Io;
        }
    }
#else
    QDirIterator iterator(path,
                          QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System);
    while (iterator.hasNext())
    {
        iterator.next();
        onEntry(iterator.filePath());
    }
    return std::nullopt;
#endif
}
} // namespace foldersnap
