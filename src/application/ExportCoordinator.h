#pragma once

#include <atomic>
#include <memory>

#include <QFutureWatcher>
#include <QObject>

#include "application/ExportJob.h"

namespace foldersnap
{
class ExportCoordinator final : public QObject
{
    Q_OBJECT

  public:
    explicit ExportCoordinator(QObject *parent = nullptr);
    ~ExportCoordinator() override;

    void start(ExportJobRequest request);
    void cancel();
    [[nodiscard]] bool isActive() const;

  signals:
    void activeChanged(bool active);
    void succeeded(const foldersnap::ExportJobResult &result);
    void failed(const QString &error);
    void cancelled();

  private:
    std::unique_ptr<QFutureWatcher<ExportJobResult>> m_watcher;
    std::shared_ptr<std::atomic_bool> m_cancelRequested;
    bool m_shuttingDown{false};
};
} // namespace foldersnap
