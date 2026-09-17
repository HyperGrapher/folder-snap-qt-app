#include "application/ExportCoordinator.h"

#include <utility>

#include <QtConcurrent>

namespace foldersnap
{
ExportCoordinator::ExportCoordinator(QObject *parent)
    : QObject(parent), m_watcher(std::make_unique<QFutureWatcher<ExportJobResult>>(this))
{
    connect(m_watcher.get(), &QFutureWatcher<ExportJobResult>::finished, this,
            [this]()
            {
                const ExportJobResult result = m_watcher->result();
                m_cancelRequested.reset();
                if (m_shuttingDown)
                {
                    return;
                }
                emit activeChanged(false);
                if (result.cancelled)
                {
                    emit cancelled();
                }
                else if (!result.succeeded)
                {
                    emit failed(result.error);
                }
                else
                {
                    emit succeeded(result);
                }
            });
}

ExportCoordinator::~ExportCoordinator()
{
    m_shuttingDown = true;
    cancel();
    m_watcher->waitForFinished();
}

void ExportCoordinator::start(ExportJobRequest request)
{
    if (isActive())
    {
        emit failed("An export is already in progress.");
        return;
    }

    m_cancelRequested = std::make_shared<std::atomic_bool>(false);
    const std::shared_ptr<std::atomic_bool> cancelRequested = m_cancelRequested;
    emit activeChanged(true);
    m_watcher->setFuture(QtConcurrent::run(
        [request = std::move(request), cancelRequested]() {
            return ExportJob::run(request, [cancelRequested]() { return cancelRequested->load(); });
        }));
}

void ExportCoordinator::cancel()
{
    if (m_cancelRequested)
    {
        m_cancelRequested->store(true);
    }
}

bool ExportCoordinator::isActive() const
{
    return m_watcher->isRunning();
}
} // namespace foldersnap
