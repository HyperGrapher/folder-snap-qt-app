#include "storage/SnapshotStore.h"

#include <algorithm>
#include <limits>
#include <utility>

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <zlib.h>

#include "domain/DomainError.h"
#include "domain/JsonCodec.h"
#include "storage/AtomicFile.h"

namespace foldersnap
{
namespace
{
constexpr qsizetype kMaximumCompressedSnapshotBytes = 1024 * 1024 * 1024;
constexpr int kCompressionBufferBytes = 64 * 1024;

[[noreturn]] void compressionError(const QString &message)
{
    throw DomainError(ErrorCode::InvalidData, message);
}

QByteArray gzipCompress(const QByteArray &input)
{
    if (input.size() > static_cast<qsizetype>(std::numeric_limits<uInt>::max()))
    {
        throw DomainError(ErrorCode::SizeLimit, "Snapshot JSON is too large to compress.");
    }

    z_stream stream{};
    if (deflateInit2(&stream, Z_BEST_SPEED, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
    {
        compressionError("Could not initialize gzip compression.");
    }

    QByteArray output;
    output.reserve(input.size());
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(input.constData()));
    stream.avail_in = static_cast<uInt>(input.size());
    int result = Z_OK;
    while (result != Z_STREAM_END)
    {
        char buffer[kCompressionBufferBytes];
        stream.next_out = reinterpret_cast<Bytef *>(buffer);
        stream.avail_out = sizeof(buffer);
        result = deflate(&stream, Z_FINISH);
        if (result != Z_OK && result != Z_STREAM_END)
        {
            deflateEnd(&stream);
            compressionError("Could not compress the snapshot payload.");
        }
        output.append(buffer, sizeof(buffer) - stream.avail_out);
    }
    deflateEnd(&stream);
    return output;
}

void checkCancelled(const SnapshotStore::CancellationCallback &cancelled)
{
    if (cancelled && cancelled())
    {
        throw DomainError(ErrorCode::Cancelled, "Snapshot loading was cancelled.");
    }
}

QByteArray gzipDecompress(const QByteArray &input, qsizetype maximumDecodedBytes,
                          const SnapshotStore::CancellationCallback &cancelled)
{
    z_stream stream{};
    if (inflateInit2(&stream, 15 + 16) != Z_OK)
    {
        compressionError("Could not initialize gzip decompression.");
    }

    QByteArray output;
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(input.constData()));
    stream.avail_in = static_cast<uInt>(input.size());
    int result = Z_OK;
    while (result != Z_STREAM_END)
    {
        checkCancelled(cancelled);
        char buffer[kCompressionBufferBytes];
        stream.next_out = reinterpret_cast<Bytef *>(buffer);
        stream.avail_out = sizeof(buffer);
        result = inflate(&stream, Z_NO_FLUSH);
        const qsizetype produced = sizeof(buffer) - stream.avail_out;
        if (produced > 0)
        {
            if (output.size() > maximumDecodedBytes - produced)
            {
                inflateEnd(&stream);
                throw DomainError(ErrorCode::SizeLimit,
                                  "Decoded snapshot payload exceeds the allowed size.");
            }
            output.append(buffer, produced);
        }
        if (result != Z_OK && result != Z_STREAM_END)
        {
            inflateEnd(&stream);
            compressionError("Snapshot payload is not a valid gzip stream.");
        }
        if (result == Z_OK && stream.avail_in == 0 && produced == 0)
        {
            inflateEnd(&stream);
            compressionError("Snapshot payload ended before the gzip stream completed.");
        }
    }
    const bool hasTrailingBytes = stream.avail_in != 0;
    inflateEnd(&stream);
    if (hasTrailingBytes)
    {
        compressionError("Snapshot payload contains trailing gzip data.");
    }
    return output;
}
} // namespace

QString snapshotPayloadPath(const StoragePaths &paths, const QString &snapshotId)
{
    validateUuid(snapshotId);
    return QDir(paths.snapshotsDirectory).filePath(snapshotId + ".snapshot");
}

SnapshotStore::SnapshotStore(StoragePaths paths, qsizetype maximumDecodedBytes)
    : m_paths(std::move(paths)), m_maximumDecodedBytes(maximumDecodedBytes)
{
    if (maximumDecodedBytes <= 0)
    {
        throw DomainError(ErrorCode::SizeLimit, "Snapshot decode limit must be positive.");
    }
}

const StoragePaths &SnapshotStore::paths() const
{
    return m_paths;
}

QString SnapshotStore::payloadPath(const QString &snapshotId) const
{
    return snapshotPayloadPath(m_paths, snapshotId);
}

QString SnapshotStore::tombstonePath(const QString &snapshotId) const
{
    return payloadPath(snapshotId) + ".deleting";
}

bool SnapshotStore::hasPayload(const QString &snapshotId) const
{
    return QFileInfo::exists(payloadPath(snapshotId));
}

qint64 SnapshotStore::saveSnapshot(const Snapshot &snapshot) const
{
    const QByteArray json = encodeSnapshot(snapshot);
    const QByteArray compressed = gzipCompress(json);
    replaceFileAtomically(payloadPath(snapshot.header.snapshotId), compressed);
    return compressed.size();
}

Snapshot SnapshotStore::loadSnapshot(const QString &snapshotId,
                                     const CancellationCallback &cancelled) const
{
    checkCancelled(cancelled);
    const QString path = payloadPath(snapshotId);
    if (!QFileInfo::exists(path))
    {
        throw DomainError(ErrorCode::MissingPayload,
                          QString("Snapshot payload is missing: %1").arg(snapshotId));
    }
    const QByteArray compressed = readFileLimited(path, kMaximumCompressedSnapshotBytes);
    checkCancelled(cancelled);
    const Snapshot snapshot =
        decodeSnapshot(gzipDecompress(compressed, m_maximumDecodedBytes, cancelled));
    checkCancelled(cancelled);
    if (snapshot.header.snapshotId != snapshotId)
    {
        throw DomainError(ErrorCode::InvalidData,
                          "Snapshot payload ID does not match its filename.");
    }
    return snapshot;
}

bool SnapshotStore::movePayloadToTombstone(const QString &snapshotId) const
{
    const QString payload = payloadPath(snapshotId);
    if (!QFileInfo::exists(payload))
    {
        return false;
    }
    const QString tombstone = tombstonePath(snapshotId);
    if (QFileInfo::exists(tombstone) || !QFile::rename(payload, tombstone))
    {
        throw DomainError(ErrorCode::Io,
                          QString("Could not tombstone snapshot payload: %1").arg(snapshotId));
    }
    return true;
}

void SnapshotStore::restoreTombstone(const QString &snapshotId) const
{
    const QString tombstone = tombstonePath(snapshotId);
    if (!QFileInfo::exists(tombstone))
    {
        return;
    }
    const QString payload = payloadPath(snapshotId);
    if (QFileInfo::exists(payload) || !QFile::rename(tombstone, payload))
    {
        throw DomainError(ErrorCode::Io,
                          QString("Could not restore snapshot payload: %1").arg(snapshotId));
    }
}

void SnapshotStore::removeTombstone(const QString &snapshotId) const
{
    const QString tombstone = tombstonePath(snapshotId);
    if (QFileInfo::exists(tombstone) && !QFile::remove(tombstone))
    {
        throw DomainError(ErrorCode::Io,
                          QString("Could not remove snapshot tombstone: %1").arg(snapshotId));
    }
}
} // namespace foldersnap
