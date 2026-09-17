#pragma once

#include <memory>
#include <optional>

#include <QFutureWatcher>
#include <QHash>
#include <QObject>
#include <QStringList>

#include "scanner/MetadataScanner.h"
#include "storage/HistoryStore.h"
#include "storage/StoragePaths.h"

namespace foldersnap
{
struct ScanJobRequest
{
    ScanRequest scan;
    StoragePaths paths;
    int retention{50};
};

struct ScanJobResult
{
    SnapshotCommitResult commit;
    QString rootId;
    SnapshotTrigger trigger{SnapshotTrigger::Manual};
    QString error;
};

class ScanCoordinator final : public QObject
{
    Q_OBJECT

  public:
    explicit ScanCoordinator(QObject *parent = nullptr);
    ~ScanCoordinator() override;

    void request(ScanJobRequest request);
    void cancelRoot(const QString &rootId);
    void cancelAll();
    [[nodiscard]] bool isActive(const QString &rootId) const;
    [[nodiscard]] int activeCount() const;

  signals:
    void activeChanged(const QString &rootId, bool active);
    void progressChanged(const QString &rootId, int progress);
    void succeeded(const foldersnap::ScanJobResult &result);
    void failed(const QString &rootId, const QString &error);
    void cancelled(const QString &rootId);

  private:
    struct PendingRequests
    {
        std::optional<ScanJobRequest> manual;
        std::optional<ScanJobRequest> scheduled;
    };

    void dispatch();
    void start(ScanJobRequest request);
    [[nodiscard]] std::optional<ScanJobRequest> takeNextPending();

    QHash<QString, PendingRequests> m_pending;
    QStringList m_pendingOrder;
    QHash<QString, QFutureWatcher<ScanJobResult> *> m_active;
    bool m_shuttingDown{false};
};
} // namespace foldersnap
