#pragma once

#include <QList>
#include <QString>

#include "domain/Configuration.h"
#include "storage/StoragePaths.h"

namespace foldersnap
{
template <typename Value> struct LoadResult
{
    Value value;
    QString preservedCorruptFile;

    [[nodiscard]] bool restoredDefaults() const
    {
        return !preservedCorruptFile.isEmpty();
    }
};

class ConfigurationStore final
{
  public:
    explicit ConfigurationStore(StoragePaths paths);

    [[nodiscard]] const StoragePaths &paths() const;
    [[nodiscard]] LoadResult<Configuration> loadConfiguration() const;
    void saveConfiguration(const Configuration &configuration) const;
    [[nodiscard]] LoadResult<QList<HistoryRecord>> loadHistoryIndex() const;
    void saveHistoryIndex(const QList<HistoryRecord> &records) const;

  private:
    StoragePaths m_paths;
};
} // namespace foldersnap
