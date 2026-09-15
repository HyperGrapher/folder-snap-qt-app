#include <algorithm>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOperatingSystemVersion>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
#include <QtTest>

#include <windows.h>
#include <psapi.h>

#include "AppState.h"
#include "WindowsWindowController.h"

Q_IMPORT_QML_PLUGIN(FolderSnapPlugin)

namespace
{
struct FrameTiming
{
    QElapsedTimer clock;
    std::mutex mutex;
    std::vector<double> intervals;
    qint64 previousFrame{0};
};
double cpuSeconds()
{
    FILETIME created{}, exited{}, kernel{}, user{};
    GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    const auto seconds = [](FILETIME value)
    {
        ULARGE_INTEGER ticks{};
        ticks.LowPart = value.dwLowDateTime;
        ticks.HighPart = value.dwHighDateTime;
        return static_cast<double>(ticks.QuadPart) / 10000000.0;
    };
    return seconds(kernel) + seconds(user);
}
QJsonObject memorySample()
{
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    GetProcessMemoryInfo(GetCurrentProcess(),
                         reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters), sizeof(counters));
    return {{"workingSetMiB", static_cast<double>(counters.WorkingSetSize) / 1048576},
            {"privateMiB", static_cast<double>(counters.PrivateUsage) / 1048576}};
}
} // namespace

class WindowTest final : public QObject
{
    Q_OBJECT
  private:
    AppState *m_state{nullptr};
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    QQuickWindow *m_window{nullptr};
    std::unique_ptr<WindowsWindowController> m_controller;
    QString m_outputDirectory;

    QObject *pageHost() const
    {
        return m_window->findChild<QObject *>("pageHost");
    }
    void capture(const QString &name)
    {
        const QImage screenshot = m_window->grabWindow();
        QVERIFY2(!screenshot.isNull(), "The native window must render a real image");
        QVERIFY(screenshot.save(m_outputDirectory + "/" + name + ".png"));
    }
    QJsonObject measure(const QString &scenario, bool interact)
    {
        QTest::qWait(1500);
        // The render callback may finish after disconnect; it shares this bounded sample's
        // lifetime.
        const auto timing = std::make_shared<FrameTiming>();
        timing->clock.start();
        const auto connection = connect(
            m_window, &QQuickWindow::frameSwapped, this,
            [timing]()
            {
                const std::lock_guard<std::mutex> lock(timing->mutex);
                const qint64 currentFrame = timing->clock.nsecsElapsed();
                if (timing->previousFrame != 0)
                {
                    timing->intervals.push_back(
                        static_cast<double>(currentFrame - timing->previousFrame) / 1000000);
                }
                timing->previousFrame = currentFrame;
            },
            Qt::DirectConnection);
        const double startCpu = cpuSeconds();
        QJsonArray memoryCheckpoints;
        memoryCheckpoints.append(memorySample());
        QElapsedTimer elapsed;
        elapsed.start();
        int switches = 0;
        while (elapsed.elapsed() < 60000)
        {
            if (interact)
            {
                m_state->setSelectedSection(static_cast<AppState::Section>(switches % 4));
                ++switches;
            }
            QTest::qWait(250);
            if (elapsed.elapsed() / 15000 >= memoryCheckpoints.size())
            {
                memoryCheckpoints.append(memorySample());
            }
        }
        disconnect(connection);
        std::vector<double> intervals;
        {
            const std::lock_guard<std::mutex> lock(timing->mutex);
            intervals = timing->intervals;
        }
        QJsonObject sample = memorySample();
        sample["scenario"] = scenario;
        sample["durationSeconds"] = elapsed.elapsed() / 1000.0;
        sample["cpuPercentOneCore"] =
            (cpuSeconds() - startCpu) / (elapsed.elapsed() / 1000.0) * 100.0;
        sample["frameIntervals"] = static_cast<int>(intervals.size());
        sample["pageChanges"] = switches;
        sample["memoryCheckpoints"] = memoryCheckpoints;
        if (!intervals.empty())
        {
            std::sort(intervals.begin(), intervals.end());
            sample["medianFrameIntervalMs"] = intervals[intervals.size() / 2];
            sample["p95FrameIntervalMs"] =
                intervals[static_cast<size_t>((intervals.size() - 1) * 0.95)];
        }
        qInfo().noquote() << QJsonDocument(sample).toJson(QJsonDocument::Compact);
        return sample;
    }

  private slots:
    void initTestCase()
    {
        QQuickStyle::setStyle("Basic");
        m_engine = std::make_unique<QQmlApplicationEngine>();
        m_engine->loadFromModule("FolderSnap", "Main");
        QVERIFY(!m_engine->rootObjects().isEmpty());
        m_window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().first());
        QVERIFY(m_window);
        m_state = qobject_cast<AppState *>(m_window->property("appState").value<QObject *>());
        QVERIFY(m_state);
        m_controller = std::make_unique<WindowsWindowController>(*m_window);
        m_window->setProperty("windowController", QVariant::fromValue(m_controller.get()));
        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window));
        QVERIFY(pageHost());
        auto *shader = m_window->findChild<QQuickItem *>("ambientShader");
        QVERIFY(shader);
        if (qEnvironmentVariable("QT_QUICK_BACKEND") == "software")
        {
            QVERIFY(!shader->isVisible());
        }
        QVERIFY(m_window->height() <= m_window->screen()->availableGeometry().height());
        m_outputDirectory = QCoreApplication::applicationDirPath() + "/verification/scale-" +
                            QString::number(m_window->devicePixelRatio(), 'f', 2);
        if (qEnvironmentVariable("QT_QUICK_BACKEND") == "software")
        {
            m_outputDirectory += "-software";
        }
        QVERIFY(QDir().mkpath(m_outputDirectory));
        qInfo() << "Window scale" << m_window->devicePixelRatio() << "screen"
                << m_window->screen()->geometry();
    }
    void renderPagesAndInteractions()
    {
        const QStringList names{"overview", "folders", "compare", "settings"};
        for (int index = 0; index < 4; ++index)
        {
            m_state->setSelectedSection(static_cast<AppState::Section>(index));
            QTRY_VERIFY(!pageHost()->property("isTransitioning").toBool());
            QTest::qWait(250);
            if (index == 0)
            {
                capture(names[index]);
            }
        }
        auto *toggle = m_window->findChild<QQuickItem *>("reducedMotionToggle");
        QVERIFY(toggle);
        toggle->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(m_window, Qt::Key_Space);
        QVERIFY(m_state->reducedMotion());
        QTest::keyClick(m_window, Qt::Key_Space);
        QVERIFY(!m_state->reducedMotion());
        m_state->setSelectedSection(AppState::Section::Overview);
        QTRY_VERIFY(!pageHost()->property("isTransitioning").toBool());
        auto *take = m_window->findChild<QQuickItem *>("overviewSnapshotButton");
        QVERIFY(take);
        take->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(m_state->property("scanning").toBool());
        QTRY_VERIFY_WITH_TIMEOUT(!m_state->property("scanning").toBool(), 5000);
        m_state->setProperty("toast", "");
        m_state->setSelectedSection(AppState::Section::Compare);
        m_state->setProperty("beforeId", 4);
        m_state->setProperty("afterId", 5);
        m_state->setProperty("comparing", true);
        QTRY_VERIFY(m_state->property("comparisonReady").toBool());
        QTest::qWait(150);
        for (const QString &sheet :
             {"add", "folder", "detail", "warnings", "exportComparison", "cleanup", "delete"})
        {
            m_state->setProperty("sheet", sheet);
            QTest::qWait(220);
            m_state->setProperty("sheet", "");
            QTest::qWait(130);
        }
        m_state->setProperty("scenario", "Empty library");
        m_state->setSelectedSection(AppState::Section::Overview);
        QTRY_VERIFY(!pageHost()->property("isTransitioning").toBool());
        m_state->setProperty("scenario", "Sample library");
    }
    void resizeAndWindowStates()
    {
        const QSize minimumSize = m_window->minimumSize();
        m_window->resize(minimumSize);
        QTRY_COMPARE(m_window->size(), minimumSize);
        for (int index = 0; index < 4; ++index)
        {
            m_state->setSelectedSection(static_cast<AppState::Section>(index));
            QTRY_VERIFY(!pageHost()->property("isTransitioning").toBool());
            QTest::qWait(100);
        }
        m_state->setSelectedSection(AppState::Section::Compare);
        m_state->setProperty("beforeId", 4);
        m_state->setProperty("afterId", 5);
        m_state->setProperty("comparisonReady", true);
        QTRY_VERIFY(!pageHost()->property("isTransitioning").toBool());
        m_state->setProperty("sheet", "folder");
        QTest::qWait(200);
        auto *dialog = m_window->findChild<QObject *>("previewDialog");
        QVERIFY(dialog);
        QVERIFY(dialog->property("height").toReal() < m_window->height());
        m_state->setProperty("sheet", "");
        QTest::qWait(150);
        m_window->showMaximized();
        QTRY_COMPARE(m_window->visibility(), QWindow::Maximized);
        QTest::qWait(100);
        const QRect available = m_window->screen()->availableGeometry();
        QVERIFY2(available.contains(m_window->geometry()),
                 qPrintable(QString("Window %1,%2 %3x%4 outside work area %5,%6 %7x%8")
                                .arg(m_window->x())
                                .arg(m_window->y())
                                .arg(m_window->width())
                                .arg(m_window->height())
                                .arg(available.x())
                                .arg(available.y())
                                .arg(available.width())
                                .arg(available.height())));
        m_window->showNormal();
        QTRY_COMPARE(m_window->visibility(), QWindow::Windowed);
        m_window->showMinimized();
        QTRY_VERIFY(!m_controller->isExposed());
        m_window->showNormal();
        QVERIFY(QTest::qWaitForWindowExposed(m_window));
        m_window->resize(std::min(1100, available.width()), std::min(720, available.height()));
    }
    void nativeHitTesting()
    {
        const auto handle = reinterpret_cast<HWND>(m_window->winId());
        RECT bounds{};
        GetWindowRect(handle, &bounds);
        const int width = bounds.right - bounds.left;
        const int height = bounds.bottom - bounds.top;
        const int scale = qRound(m_window->devicePixelRatio());
        const auto hit = [&](int x, int y)
        {
            MSG message{};
            message.hwnd = handle;
            message.message = WM_NCHITTEST;
            message.lParam = MAKELPARAM(bounds.left + x, bounds.top + y);
            qintptr result = 0;
            const bool handled = m_controller->nativeEventFilter({}, &message, &result);
            return handled ? result : -1;
        };
        QCOMPARE(hit(1, 1), HTTOPLEFT);
        QCOMPARE(hit(width / 2, 1), HTTOP);
        QCOMPARE(hit(width - 1, 1), HTTOPRIGHT);
        QCOMPARE(hit(1, height / 2), HTLEFT);
        QCOMPARE(hit(width - 1, height / 2), HTRIGHT);
        QCOMPARE(hit(1, height - 1), HTBOTTOMLEFT);
        QCOMPARE(hit(width / 2, height - 1), HTBOTTOM);
        QCOMPARE(hit(width - 1, height - 1), HTBOTTOMRIGHT);
        QCOMPARE(hit(100 * scale, 24 * scale), HTCAPTION);
        QCOMPARE(hit(width - 24 * scale, 24 * scale), HTCLIENT);
        QCOMPARE(hit(width / 2, 100 * scale), HTCLIENT);
        m_window->showMaximized();
        QTRY_COMPARE(m_window->visibility(), QWindow::Maximized);
        GetWindowRect(handle, &bounds);
        QCOMPARE(hit(1, 1), HTCAPTION);
        m_window->showNormal();
        QTRY_COMPARE(m_window->visibility(), QWindow::Windowed);
    }
    void nativeCornersAndCaption()
    {
        const auto handle = reinterpret_cast<HWND>(m_window->winId());
        if (QOperatingSystemVersion::current().microVersion() < 22000)
        {
            const auto deleteRegion = [](HRGN region) { DeleteObject(region); };
            std::unique_ptr<std::remove_pointer_t<HRGN>, decltype(deleteRegion)> region(
                CreateRectRgn(0, 0, 0, 0), deleteRegion);
            QVERIFY(region);
            QTRY_VERIFY(GetWindowRgn(handle, region.get()) != ERROR);
            QVERIFY(!PtInRegion(region.get(), 0, 0));
            QVERIFY(PtInRegion(region.get(), 50, 50));
        }
        SendMessage(handle, WM_NCLBUTTONDBLCLK, HTCAPTION, 0);
        QTRY_COMPARE(m_window->visibility(), QWindow::Maximized);
        SendMessage(handle, WM_NCLBUTTONDBLCLK, HTCAPTION, 0);
        QTRY_COMPARE(m_window->visibility(), QWindow::Windowed);
    }
    void windowButtons()
    {
        const auto clickButton = [this](const QString &name)
        {
            auto *button = m_window->findChild<QQuickItem *>(name);
            if (!button)
            {
                return false;
            }
            QTest::mouseClick(
                m_window, Qt::LeftButton, Qt::NoModifier,
                button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint());
            return true;
        };
        QVERIFY(clickButton("window-maximize"));
        QTRY_COMPARE(m_window->visibility(), QWindow::Maximized);
        QVERIFY(clickButton("window-restore"));
        QTRY_COMPARE(m_window->visibility(), QWindow::Windowed);
        QVERIFY(clickButton("window-minimize"));
        QTRY_COMPARE(m_window->visibility(), QWindow::Minimized);
        m_window->showNormal();
        QVERIFY(QTest::qWaitForWindowExposed(m_window));
    }
    void performance()
    {
        if (!qEnvironmentVariableIsSet("FOLDERSNAP_MEASURE"))
        {
            QSKIP("Set FOLDERSNAP_MEASURE=1 for four 60-second resource samples");
        }
        for (int index = 0; index < 4; ++index)
        {
            m_state->setSelectedSection(static_cast<AppState::Section>(index));
            QTest::qWait(800);
        }
        m_state->setSelectedSection(AppState::Section::Overview);
        m_state->setReducedMotion(false);
        QJsonArray samples;
        const QString scenario = qEnvironmentVariable("FOLDERSNAP_MEASURE_SCENARIO");
        if (scenario.isEmpty() || scenario == "ambient")
        {
            samples.append(measure("ambient", false));
        }
        if (scenario.isEmpty() || scenario == "reduced-motion")
        {
            m_state->setReducedMotion(true);
            samples.append(measure("reduced-motion", false));
        }
        if (scenario.isEmpty() || scenario == "minimized")
        {
            m_state->setReducedMotion(false);
            m_window->showMinimized();
            samples.append(measure("minimized", false));
        }
        if (scenario.isEmpty() || scenario == "navigation-and-widgets")
        {
            m_state->setReducedMotion(false);
            m_window->showNormal();
            QVERIFY(QTest::qWaitForWindowExposed(m_window));
            samples.append(measure("navigation-and-widgets", true));
        }
        QVERIFY2(!samples.isEmpty(), "Unknown FOLDERSNAP_MEASURE_SCENARIO");
        QFile output(m_outputDirectory + "/performance" +
                     (scenario.isEmpty() ? QString{} : "-" + scenario) + ".json");
        QVERIFY(output.open(QIODevice::WriteOnly));
        output.write(QJsonDocument(samples).toJson());
    }
    void cleanupTestCase()
    {
        if (m_window)
        {
            m_window->showNormal();
            QVERIFY(QTest::qWaitForWindowExposed(m_window));
            auto *close = m_window->findChild<QQuickItem *>("window-close");
            QVERIFY(close);
            QTest::mouseClick(
                m_window, Qt::LeftButton, Qt::NoModifier,
                close->mapToScene(QPointF(close->width() / 2, close->height() / 2)).toPoint());
            QTRY_VERIFY(!m_window->isVisible());
        }
        m_controller.reset();
        m_engine.reset();
        m_window = nullptr;
    }
};
QTEST_MAIN(WindowTest)
#include "tst_window.moc"
