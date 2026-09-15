#pragma once

#include <QtTypes>

#include "domain/Snapshot.h"
#include "storage/StoragePaths.h"

namespace foldersnap
{
inline constexpr qsizetype kMaximumDecodedSnapshotBytes = 1024 * 1024 * 1024;

[[nodiscard]] QString snapshotPayloadPath(const StoragePaths &paths, const QString &snapshotId);

class SnapshotStore final
{
  public:
    explicit SnapshotStore(StoragePaths paths,
                           qsizetype maximumDecodedBytes = kMaximumDecodedSnapshotBytes);

    [[nodiscard]] const StoragePaths &paths() const;
    [[nodiscard]] QString payloadPath(const QString &snapshotId) const;
    [[nodiscard]] bool hasPayload(const QString &snapshotId) const;
    [[nodiscard]] qint64 saveSnapshot(const Snapshot &snapshot) const;
    [[nodiscard]] Snapshot loadSnapshot(const QString &snapshotId) const;

  private:
    StoragePaths m_paths;
    qsizetype m_maximumDecodedBytes;
};
} // namespace foldersnap
