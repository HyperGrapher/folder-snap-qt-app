#include "application/ScanCoordinator.h"

#include <exception>
#include <utility>

#include <QPromise>
#include <QtConcurrent>

#include "domain/DomainError.h"

namespace foldersnap
{
namespace
{
ScanJobResult runScan(const ScanJobRequest &request, QPromise<ScanJobResult> &promise)
{
    ScanJobResult result;
    result.rootId = request.scan.rootId;
    result.trigger = request.scan.trigger;
    promise.setProgressRange(0, 100);
    try
    {
        const ScanResult scan = MetadataScanner::scan(
            request.scan, [&promise](int value) { promise.setProgressValue(value); },
            [&promise]() { return promise.isCanceled(); });
        if (scan.cancelled || promise.isCanceled())
        {
            return result;
        }
        if (!scan.error.isEmpty())
        {
            result.error = scan.error;
            return result;
        }
        result.commit =
            HistoryStore(request.paths).commitSnapshot(scan.snapshot, request.retention);
        promise.setProgressValue(100);
    }
    catch (const DomainError &error)
    {
        result.error = error.message();
    }
    catch (const std::exception &error)
    {
        result.error = QString::fromUtf8(error.what());
    }
    catch (...)
    {
        result.error = "Unexpected snapshot failure.";
    }
    return result;
}
} // namespace

ScanCoordinator::ScanCoordinator(QObject *parent) : QObject(parent) {}

ScanCoordinator::~ScanCoordinator()
{
    m_shuttingDown = true;
    m_pending.clear();
    m_pendingOrder.clear();
    const auto watchers = m_active.values();
    for (QFutureWatcher<ScanJobResult> *watcher : watchers)
    {
        disconnect(watcher, nullptr, this, nullptr);
        watcher->cancel();
    }
    for (QFutureWatcher<ScanJobResult> *watcher : watchers)
    {
        watcher->waitForFinished();
    }
}

void ScanCoordinator::request(ScanJobRequest request)
{
    if (m_shuttingDown)
    {
        return;
    }
    const QString rootId = request.scan.rootId;
    PendingRequests &pending = m_pending[rootId];
    if (request.scan.trigger == SnapshotTrigger::Manual)
    {
        pending.manual = std::move(request);
    }
    else
    {
        pending.scheduled = std::move(request);
    }
    if (!m_pendingOrder.contains(rootId))
    {
        m_pendingOrder.append(rootId);
    }
    dispatch();
}

void ScanCoordinator::cancelRoot(const QString &rootId)
{
    m_pending.remove(rootId);
    m_pendingOrder.removeAll(rootId);
    if (QFutureWatcher<ScanJobResult> *watcher = m_active.value(rootId))
    {
        watcher->cancel();
    }
}

void ScanCoordinator::cancelAll()
{
    m_pending.clear();
    m_pendingOrder.clear();
    for (QFutureWatcher<ScanJobResult> *watcher : std::as_const(m_active))
    {
        watcher->cancel();
    }
}

bool ScanCoordinator::isActive(const QString &rootId) const
{
    return m_active.contains(rootId);
}

int ScanCoordinator::activeCount() const
{
    return m_active.size();
}

void ScanCoordinator::dispatch()
{
    while (!m_shuttingDown && m_active.size() < 2)
    {
        std::optional<ScanJobRequest> next = takeNextPending();
        if (!next)
        {
            return;
        }
        start(std::move(*next));
    }
}

std::optional<ScanJobRequest> ScanCoordinator::takeNextPending()
{
    for (const SnapshotTrigger trigger : {SnapshotTrigger::Manual, SnapshotTrigger::Scheduled})
    {
        for (const QString &rootId : std::as_const(m_pendingOrder))
        {
            if (m_active.contains(rootId))
            {
                continue;
            }
            auto pending = m_pending.find(rootId);
            if (pending == m_pending.end())
            {
                continue;
            }
            std::optional<ScanJobRequest> &request =
                trigger == SnapshotTrigger::Manual ? pending->manual : pending->scheduled;
            if (!request)
            {
                continue;
            }
            ScanJobRequest result = std::move(*request);
            request.reset();
            if (!pending->manual && !pending->scheduled)
            {
                m_pending.erase(pending);
                m_pendingOrder.removeAll(rootId);
            }
            return result;
        }
    }
    return std::nullopt;
}

void ScanCoordinator::start(ScanJobRequest request)
{
    const QString rootId = request.scan.rootId;
    auto *watcher = new QFutureWatcher<ScanJobResult>(this);
    m_active.insert(rootId, watcher);
    connect(watcher, &QFutureWatcher<ScanJobResult>::progressValueChanged, this,
            [this, rootId](int progress) { emit progressChanged(rootId, progress); });
    connect(watcher, &QFutureWatcher<ScanJobResult>::finished, this,
            [this, watcher, rootId]()
            {
                const bool wasCancelled = watcher->future().isCanceled();
                std::optional<ScanJobResult> result;
                if (!wasCancelled && watcher->future().resultCount() > 0)
                {
                    result = watcher->result();
                }
                m_active.remove(rootId);
                watcher->deleteLater();
                emit activeChanged(rootId, false);
                if (wasCancelled || !result)
                {
                    emit cancelled(rootId);
                }
                else if (!result->error.isEmpty())
                {
                    emit failed(rootId, result->error);
                }
                else
                {
                    emit succeeded(*result);
                }
                dispatch();
            });
    emit activeChanged(rootId, true);
    watcher->setFuture(QtConcurrent::run(
        [request = std::move(request)](QPromise<ScanJobResult> &promise) mutable
        {
            ScanJobResult result = runScan(request, promise);
            if (!promise.isCanceled())
            {
                promise.addResult(std::move(result));
            }
        }));
}
} // namespace foldersnap
