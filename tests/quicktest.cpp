#include <QQmlExtensionPlugin>
#include <QFile>
#include <QQuickStyle>
#include <QTemporaryDir>
#include <QtQuickTest/quicktest.h>

#include "domain/Configuration.h"
#include "paths/WindowsPaths.h"
#include "storage/ConfigurationStore.h"
#include "storage/StoragePaths.h"

namespace
{
QTemporaryDir &dataDirectory()
{
    static QTemporaryDir directory("FolderSnap-QmlTests-XXXXXX");
    return directory;
}

QTemporaryDir &watchedDirectory()
{
    static QTemporaryDir directory("FolderSnap-QmlWatched-XXXXXX");
    return directory;
}

[[maybe_unused]] const bool kUseIsolatedDataDirectory = []
{
    if (!dataDirectory().isValid() || !watchedDirectory().isValid())
    {
        qFatal("Could not create QML test directories.");
    }
    qputenv("FOLDERSNAP_DATA_DIR", dataDirectory().path().toUtf8());
    return true;
}();

void seedWatchedFolder()
{
    QFile file(watchedDirectory().filePath("tracked.txt"));
    if (!file.open(QIODevice::WriteOnly) || file.write("FolderSnap UI test") < 0)
    {
        qFatal("Could not create the QML test file.");
    }

    const foldersnap::RootPath normalized =
        foldersnap::normalizeRootPath(watchedDirectory().path());
    foldersnap::WatchedRoot root;
    root.rootId = foldersnap::createId();
    root.displayName = "QML test folder";
    root.path = normalized.displayPath;
    root.normalizedPath = normalized.identityPath;

    foldersnap::Configuration configuration;
    configuration.roots.append(root);
    const foldersnap::StoragePaths paths =
        foldersnap::StoragePaths::fromDataDirectory(dataDirectory().path());
    foldersnap::ConfigurationStore(paths).saveConfiguration(configuration);
}
} // namespace

Q_IMPORT_QML_PLUGIN(FolderSnapPlugin)
class TestSetup final : public QObject
{
    Q_OBJECT
  public slots:
    void applicationAvailable()
    {
        QQuickStyle::setStyle("Basic");
        seedWatchedFolder();
    }
};
QUICK_TEST_MAIN_WITH_SETUP(foldersnap, TestSetup)
#include "quicktest.moc"
