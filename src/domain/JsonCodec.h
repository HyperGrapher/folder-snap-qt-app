#pragma once

#include <QByteArray>
#include <QList>

#include "domain/Configuration.h"
#include "domain/Snapshot.h"

namespace foldersnap
{
[[nodiscard]] QByteArray encodeSnapshot(const Snapshot &snapshot);
[[nodiscard]] Snapshot decodeSnapshot(const QByteArray &json);
[[nodiscard]] QByteArray encodeConfiguration(const Configuration &configuration);
[[nodiscard]] Configuration decodeConfiguration(const QByteArray &json);
[[nodiscard]] QByteArray encodeHistoryIndex(const QList<HistoryRecord> &records);
[[nodiscard]] QList<HistoryRecord> decodeHistoryIndex(const QByteArray &json);
} // namespace foldersnap
