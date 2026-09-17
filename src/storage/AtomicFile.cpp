#include "storage/AtomicFile.h"

#include <algorithm>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUuid>

#include "domain/DomainError.h"

namespace foldersnap
{
namespace
{
constexpr qsizetype kWriteChunkBytes = 64 * 1024;

[[noreturn]] void fileError(const QString &action, const QString &path, const QString &detail)
{
    throw DomainError(ErrorCode::Io, QString("Could not %1 '%2': %3").arg(action, path, detail));
}

void createDirectory(const QString &path)
{
    if (!QDir().mkpath(path))
    {
        fileError("create directory", path, "the operating system rejected the path");
    }
}
} // namespace

QByteArray readFileLimited(const QString &path, qsizetype maximumBytes)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        fileError("open", path, file.errorString());
    }
    if (file.size() > maximumBytes)
    {
        throw DomainError(ErrorCode::SizeLimit,
                          QString("File '%1' exceeds the allowed size.").arg(path));
    }

    const QByteArray contents = file.readAll();
    if (contents.size() > maximumBytes)
    {
        throw DomainError(ErrorCode::SizeLimit,
                          QString("File '%1' exceeds the allowed size.").arg(path));
    }
    if (file.error() != QFile::NoError)
    {
        fileError("read", path, file.errorString());
    }
    return contents;
}

void replaceFileAtomically(const QString &path, const QByteArray &contents)
{
    static_cast<void>(replaceFileAtomically(path, contents, {}));
}

bool replaceFileAtomically(const QString &path, const QByteArray &contents,
                           const std::function<bool()> &cancelled)
{
    if (cancelled && cancelled())
    {
        return false;
    }
    const QFileInfo fileInfo(path);
    createDirectory(fileInfo.absolutePath());

    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly))
    {
        fileError("open for replacement", path, file.errorString());
    }
    qsizetype offset = 0;
    while (offset < contents.size())
    {
        if (cancelled && cancelled())
        {
            file.cancelWriting();
            return false;
        }
        const qsizetype bytesToWrite = std::min(kWriteChunkBytes, contents.size() - offset);
        if (file.write(contents.constData() + offset, bytesToWrite) != bytesToWrite)
        {
            file.cancelWriting();
            fileError("write", path, file.errorString());
        }
        offset += bytesToWrite;
    }
    if (cancelled && cancelled())
    {
        file.cancelWriting();
        return false;
    }
    if (!file.commit())
    {
        fileError("replace", path, file.errorString());
    }
    return true;
}

QString preserveCorruptFile(const QString &path, const StoragePaths &paths)
{
    if (!QFileInfo::exists(path))
    {
        return {};
    }

    createDirectory(paths.corruptDirectory);
    const QFileInfo source(path);
    const QString timestamp = QDateTime::currentDateTimeUtc().toString("yyyyMMddTHHmmsszzzZ");
    const QString filename =
        QString("%1.%2.%3.corrupt")
            .arg(source.fileName(), timestamp, QUuid::createUuid().toString(QUuid::Id128));
    const QString destination = QDir(paths.corruptDirectory).filePath(filename);
    if (!QFile::rename(path, destination))
    {
        fileError("preserve corrupt file", path,
                  "the file could not be moved to the corrupt directory");
    }
    return destination;
}
} // namespace foldersnap
