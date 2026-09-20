#include "platform/windows/WindowsStartupController.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QString>

namespace
{
constexpr auto kRunKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr auto kValueName = "FolderSnap";

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage)
    {
        *errorMessage = message;
    }
}
} // namespace

bool WindowsStartupController::setEnabled(bool enabled, QString *errorMessage)
{
#ifdef Q_OS_WIN
    const QString executablePath = QCoreApplication::applicationFilePath();
    if (enabled && executablePath.isEmpty())
    {
        setError(errorMessage, "FolderSnap's executable path is unavailable.");
        return false;
    }

    QSettings settings(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
    if (enabled)
    {
        const QString command =
            QStringLiteral("\"%1\" --background").arg(QDir::toNativeSeparators(executablePath));
        settings.setValue(QString::fromLatin1(kValueName), command);
    }
    else
    {
        settings.remove(QString::fromLatin1(kValueName));
    }
    settings.sync();
    if (settings.status() != QSettings::NoError)
    {
        setError(errorMessage, "Windows could not update FolderSnap's startup setting.");
        return false;
    }
    return true;
#else
    Q_UNUSED(enabled);
    Q_UNUSED(errorMessage);
    return true;
#endif
}
