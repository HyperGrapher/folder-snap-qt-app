#include "platform/windows/SingleInstance.h"

#include <utility>

#include <QByteArray>
#include <QLocalSocket>
#include <QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace
{
constexpr auto kActivationMessage = "activate\n";
}

SingleInstance::SingleInstance(QString instanceName, QObject *parent)
    : QObject(parent), m_instanceName(std::move(instanceName))
{
    connect(&m_server, &QLocalServer::newConnection, this, &SingleInstance::handleConnections);
}

SingleInstance::~SingleInstance()
{
    m_server.close();
#ifdef Q_OS_WIN
    if (m_mutex)
    {
        CloseHandle(static_cast<HANDLE>(m_mutex));
    }
#endif
}

SingleInstance::AcquireResult SingleInstance::acquire()
{
#ifdef Q_OS_WIN
    const QString nativeMutexName = mutexName();
    HANDLE mutex = CreateMutexW(nullptr, TRUE, reinterpret_cast<LPCWSTR>(nativeMutexName.utf16()));
    if (!mutex)
    {
        return AcquireResult::Failed;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(mutex);
        return forwardActivation() ? AcquireResult::Forwarded : AcquireResult::Failed;
    }
    m_mutex = mutex;
#endif

    QLocalServer::removeServer(serverName());
    if (!m_server.listen(serverName()))
    {
#ifdef Q_OS_WIN
        CloseHandle(static_cast<HANDLE>(m_mutex));
        m_mutex = nullptr;
#endif
        return AcquireResult::Failed;
    }
    return AcquireResult::Primary;
}

bool SingleInstance::takePendingActivation()
{
    const bool pending = m_pendingActivation;
    m_pendingActivation = false;
    return pending;
}

QString SingleInstance::serverName() const
{
    return m_instanceName + QStringLiteral(".SingleInstance.Channel");
}

QString SingleInstance::mutexName() const
{
    return QStringLiteral("Local\\") + m_instanceName + QStringLiteral(".SingleInstance");
}

bool SingleInstance::forwardActivation() const
{
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        QLocalSocket socket;
        socket.connectToServer(serverName());
        if (socket.waitForConnected(350))
        {
            if (socket.write(kActivationMessage) == qstrlen(kActivationMessage) &&
                socket.waitForBytesWritten(350))
            {
                return true;
            }
        }
        QThread::msleep(50);
    }
    return false;
}

void SingleInstance::handleConnections()
{
    while (QLocalSocket *socket = m_server.nextPendingConnection())
    {
        connect(socket, &QLocalSocket::readyRead, this,
                [this, socket]
                {
                    const QByteArray message = socket->readAll();
                    if (message.contains("activate"))
                    {
                        m_pendingActivation = true;
                        emit activationRequested();
                    }
                    socket->disconnectFromServer();
                });
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
    }
}
