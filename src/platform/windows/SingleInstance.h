#pragma once

#include <QString>

#include <QLocalServer>
#include <QObject>

#ifdef Q_OS_WIN
using SingleInstanceMutexHandle = void *;
#endif

class SingleInstance final : public QObject
{
    Q_OBJECT

  public:
    enum class AcquireResult
    {
        Primary,
        Forwarded,
        Failed
    };
    Q_ENUM(AcquireResult)

    explicit SingleInstance(QString instanceName = QStringLiteral("FolderSnap"),
                            QObject *parent = nullptr);
    ~SingleInstance() override;

    [[nodiscard]] AcquireResult acquire();
    [[nodiscard]] bool takePendingActivation();

  signals:
    void activationRequested();

  private:
    [[nodiscard]] QString serverName() const;
    [[nodiscard]] QString mutexName() const;
    [[nodiscard]] bool forwardActivation() const;
    void handleConnections();

    QString m_instanceName;
    QLocalServer m_server;
    bool m_pendingActivation{false};
#ifdef Q_OS_WIN
    SingleInstanceMutexHandle m_mutex{nullptr};
#endif
};
