#include <QQmlExtensionPlugin>
#include <QDir>
#include <QQuickStyle>
#include <QUuid>
#include <QtQuickTest/quicktest.h>

namespace
{
[[maybe_unused]] const bool kUseIsolatedDataDirectory = [] {
    const QString directory = QDir(QDir::tempPath()).filePath(
        "FolderSnap-QmlTests-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    qputenv("FOLDERSNAP_DATA_DIR", directory.toUtf8());
    return true;
}();
}

Q_IMPORT_QML_PLUGIN(FolderSnapPlugin)
class TestSetup final : public QObject
{
    Q_OBJECT
  public slots:
    void applicationAvailable()
    {
        QQuickStyle::setStyle("Basic");
    }
};
QUICK_TEST_MAIN_WITH_SETUP(foldersnap, TestSetup)
#include "quicktest.moc"
