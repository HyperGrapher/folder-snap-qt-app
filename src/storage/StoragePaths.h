#pragma once

#include <QString>

namespace foldersnap
{
struct StoragePaths
{
    QString dataDirectory;
    QString configurationFile;
    QString historyIndexFile;
    QString snapshotsDirectory;
    QString corruptDirectory;

    [[nodiscard]] static StoragePaths forCurrentUser();
    [[nodiscard]] static StoragePaths fromDataDirectory(const QString &dataDirectory);
};
} // namespace foldersnap
