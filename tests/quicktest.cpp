#include <QQmlExtensionPlugin>
#include <QQuickStyle>
#include <QtQuickTest/quicktest.h>

Q_IMPORT_QML_PLUGIN(AuraPlugin)
class TestSetup final : public QObject
{
    Q_OBJECT
  public slots:
    void applicationAvailable()
    {
        QQuickStyle::setStyle("Basic");
    }
};
QUICK_TEST_MAIN_WITH_SETUP(aura, TestSetup)
#include "quicktest.moc"
