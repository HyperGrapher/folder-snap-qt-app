#include "storage/StoragePaths.h"

#include <QDir>
#include <QStandardPaths>

#include "domain/DomainError.h"

namespace foldersnap
{
StoragePaths StoragePaths::forCurrentUser()
{
    return fromDataDirectory(
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
}

StoragePaths StoragePaths::fromDataDirectory(const QString &dataDirectory)
{
    const QString cleanedDirectory = QDir::cleanPath(dataDirectory);
    if (cleanedDirectory.isEmpty() || !QDir::isAbsolutePath(cleanedDirectory))
    {
        throw DomainError(ErrorCode::InvalidPath,
                          "The FolderSnap data directory must be an absolute path.");
    }

    const QDir directory(cleanedDirectory);
    return {
        directory.absolutePath(),
        directory.filePath("configuration.json"),
        directory.filePath("history-index.json"),
        directory.filePath("snapshots"),
        directory.filePath("corrupt"),
    };
}
} // namespace foldersnap
