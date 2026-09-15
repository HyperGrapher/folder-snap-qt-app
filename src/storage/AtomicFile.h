#pragma once

#include <QtTypes>

#include <QString>

#include "storage/StoragePaths.h"

namespace foldersnap
{
[[nodiscard]] QByteArray readFileLimited(const QString &path, qsizetype maximumBytes);
void replaceFileAtomically(const QString &path, const QByteArray &contents);
[[nodiscard]] QString preserveCorruptFile(const QString &path, const StoragePaths &paths);
} // namespace foldersnap
