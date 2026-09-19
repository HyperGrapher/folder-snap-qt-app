#include "WindowsWindowController.h"

#include <memory>
#include <type_traits>

#include <QCoreApplication>
#include <QDebug>
#include <QEvent>
#include <QOperatingSystemVersion>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#endif

WindowsWindowController::WindowsWindowController(QQuickWindow &window) : m_window(window)
{
    m_window.installEventFilter(this);
    m_handle = m_window.winId();
    QCoreApplication::instance()->installNativeEventFilter(this);
#ifdef Q_OS_WIN
    const auto handle = reinterpret_cast<HWND>(m_handle);
    const LONG_PTR style = GetWindowLongPtr(handle, GWL_STYLE);
    SetWindowLongPtr(handle, GWL_STYLE,
                     style | WS_THICKFRAME | WS_CAPTION | WS_MINIMIZEBOX | WS_MAXIMIZEBOX |
                         WS_SYSMENU);
    SetWindowPos(handle, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    m_usesNativeRounding = QOperatingSystemVersion::current().microVersion() >= 22000;
#endif
    connect(&m_window, &QWindow::widthChanged, this, &WindowsWindowController::updateCorners);
    connect(&m_window, &QWindow::heightChanged, this, &WindowsWindowController::updateCorners);
    connect(&m_window, &QWindow::screenChanged, this, &WindowsWindowController::updateCorners);
    connect(&m_window, &QWindow::windowStateChanged, this,
            [this]()
            {
                updateCorners();
                emit exposedChanged();
            });
    updateCorners();
}
WindowsWindowController::~WindowsWindowController()
{
    QCoreApplication::instance()->removeNativeEventFilter(this);
    m_window.removeEventFilter(this);
}
bool WindowsWindowController::isExposed() const
{
    return m_window.isExposed() && m_window.isVisible() &&
           m_window.visibility() != QWindow::Minimized;
}
void WindowsWindowController::activate()
{
    m_window.showNormal();
    m_window.raise();
    m_window.requestActivate();
#ifdef Q_OS_WIN
    SetForegroundWindow(reinterpret_cast<HWND>(m_handle));
#endif
}
bool WindowsWindowController::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == &m_window)
    {
        if (event->type() == QEvent::Expose || event->type() == QEvent::Show ||
            event->type() == QEvent::Hide)
        {
            emit exposedChanged();
        }
        if (event->type() == QEvent::DevicePixelRatioChange)
        {
            updateCorners();
        }
    }
    return false;
}
void WindowsWindowController::updateCorners()
{
#ifdef Q_OS_WIN
    const auto handle = reinterpret_cast<HWND>(m_handle);
    const bool maximized = IsZoomed(handle) || m_window.windowState() == Qt::WindowMaximized;
    if (m_usesNativeRounding)
    {
        // The Qt MinGW SDK predates these documented Windows 11 DWM constants.
        constexpr DWORD kWindowCornerPreference = 33;
        constexpr DWORD kDoNotRound = 1;
        constexpr DWORD kRound = 2;
        const DWORD preference = maximized ? kDoNotRound : kRound;
        const HRESULT result =
            DwmSetWindowAttribute(handle, kWindowCornerPreference, &preference, sizeof(preference));
        if (FAILED(result) && !m_reportedCornerFailure)
        {
            qWarning() << "DWM corner request failed:" << result;
            m_reportedCornerFailure = true;
        }
        return;
    }
    if (maximized)
    {
        SetWindowRgn(handle, nullptr, TRUE);
        return;
    }
    RECT bounds{};
    GetWindowRect(handle, &bounds);
    const int diameter = qRound(20 * m_window.devicePixelRatio());
    const auto deleteRegion = [](HRGN region) { DeleteObject(region); };
    std::unique_ptr<std::remove_pointer_t<HRGN>, decltype(deleteRegion)> region(
        CreateRoundRectRgn(0, 0, bounds.right - bounds.left + 1, bounds.bottom - bounds.top + 1,
                           diameter, diameter),
        deleteRegion);
    if (region && SetWindowRgn(handle, region.get(), TRUE))
    {
        // Windows owns the region only after a successful SetWindowRgn call.
        region.release();
    }
    else if (!m_reportedCornerFailure)
    {
        qWarning() << "Could not apply rounded window region:" << GetLastError();
        m_reportedCornerFailure = true;
    }
#endif
}
bool WindowsWindowController::nativeEventFilter(const QByteArray &, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    const auto *nativeMessage = static_cast<MSG *>(message);
    const auto handle = reinterpret_cast<HWND>(m_handle);
    if (nativeMessage->hwnd != handle)
    {
        return false;
    }
    switch (nativeMessage->message)
    {
    case WM_NCCALCSIZE:
        if (nativeMessage->wParam)
        {
            auto *parameters = reinterpret_cast<NCCALCSIZE_PARAMS *>(nativeMessage->lParam);
            if (IsZoomed(handle))
            {
                MONITORINFO monitor{sizeof(MONITORINFO)};
                GetMonitorInfo(MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), &monitor);
                parameters->rgrc[0] = monitor.rcWork;
            }
            *result = 0;
            return true;
        }
        break;
    case WM_GETMINMAXINFO:
    {
        auto *limits = reinterpret_cast<MINMAXINFO *>(nativeMessage->lParam);
        MONITORINFO monitor{sizeof(MONITORINFO)};
        if (GetMonitorInfo(MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), &monitor))
        {
            limits->ptMaxPosition = {monitor.rcWork.left - monitor.rcMonitor.left,
                                     monitor.rcWork.top - monitor.rcMonitor.top};
            limits->ptMaxSize = {monitor.rcWork.right - monitor.rcWork.left,
                                 monitor.rcWork.bottom - monitor.rcWork.top};
            limits->ptMinTrackSize = {
                qRound(m_window.minimumWidth() * m_window.devicePixelRatio()),
                qRound(m_window.minimumHeight() * m_window.devicePixelRatio())};
            *result = 0;
            return true;
        }
        break;
    }
    case WM_NCHITTEST:
    {
        RECT bounds{};
        GetWindowRect(handle, &bounds);
        const int x = GET_X_LPARAM(nativeMessage->lParam) - bounds.left;
        const int y = GET_Y_LPARAM(nativeMessage->lParam) - bounds.top;
        const int width = bounds.right - bounds.left;
        const int height = bounds.bottom - bounds.top;
        const qreal scale = m_window.devicePixelRatio();
        const int border = qRound(6 * scale);
        if (!IsZoomed(handle) && m_window.windowState() != Qt::WindowMaximized)
        {
            const bool left = x < border;
            const bool right = x >= width - border;
            const bool top = y < border;
            const bool bottom = y >= height - border;
            if (top || bottom || left || right)
            {
                *result = top      ? (left ? HTTOPLEFT : (right ? HTTOPRIGHT : HTTOP))
                          : bottom ? (left ? HTBOTTOMLEFT : (right ? HTBOTTOMRIGHT : HTBOTTOM))
                                   : (left ? HTLEFT : HTRIGHT);
                return true;
            }
        }
        const bool isCaption =
            y < qRound(m_titleHeight * scale) && x < width - qRound(m_titleButtonsWidth * scale);
        *result = isCaption ? HTCAPTION : HTCLIENT;
        return true;
    }
    default:
        break;
    }
#else
    Q_UNUSED(message)
    Q_UNUSED(result)
#endif
    return false;
}
