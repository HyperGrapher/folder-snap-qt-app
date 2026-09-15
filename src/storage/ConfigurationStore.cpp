#include "storage/ConfigurationStore.h"

#include <utility>

#include <QFileInfo>

#include "domain/DomainError.h"
#include "domain/JsonCodec.h"
#include "storage/AtomicFile.h"
#include "storage/SnapshotStore.h"

namespace foldersnap
{
namespace
{
constexpr qsizetype kConfigurationMaximumBytes = 16 * 1024 * 1024;
constexpr qsizetype kHistoryIndexMaximumBytes = 32 * 1024 * 1024;

template <typename Value, typename Decoder>
LoadResult<Value> loadDocument(const QString &path, const StoragePaths &paths,
                               qsizetype maximumBytes, Value defaults, Decoder decoder)
{
    if (!QFileInfo::exists(path))
    {
        return {std::move(defaults), {}};
    }

    try
    {
        auto result = LoadResult<Value>{decoder(readFileLimited(path, maximumBytes)), {}};
        return result;
    }
    catch (const DomainError &error)
    {
        if (error.code() == ErrorCode::Io)
        {
            throw;
        }
        return {std::move(defaults), preserveCorruptFile(path, paths)};
    }
}
} // namespace

ConfigurationStore::ConfigurationStore(StoragePaths paths) : m_paths(std::move(paths)) {}

const StoragePaths &ConfigurationStore::paths() const
{
    return m_paths;
}

LoadResult<Configuration> ConfigurationStore::loadConfiguration() const
{
    return loadDocument(m_paths.configurationFile, m_paths, kConfigurationMaximumBytes,
                        Configuration{}, decodeConfiguration);
}

void ConfigurationStore::saveConfiguration(const Configuration &configuration) const
{
    replaceFileAtomically(m_paths.configurationFile, encodeConfiguration(configuration));
}

LoadResult<QList<HistoryRecord>> ConfigurationStore::loadHistoryIndex() const
{
    auto result = loadDocument(m_paths.historyIndexFile, m_paths, kHistoryIndexMaximumBytes,
                               QList<HistoryRecord>{}, decodeHistoryIndex);
    for (auto &record : result.value)
    {
        record.payloadAvailable =
            QFileInfo::exists(snapshotPayloadPath(m_paths, record.snapshotId));
    }
    return result;
}

void ConfigurationStore::saveHistoryIndex(const QList<HistoryRecord> &records) const
{
    replaceFileAtomically(m_paths.historyIndexFile, encodeHistoryIndex(records));
}
} // namespace foldersnap
