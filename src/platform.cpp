#include "platform.h"

#include <QWidget>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dwmapi.h>
#endif

namespace Platform {

#ifdef Q_OS_WIN
namespace {

const DWORD kDwmExtendedFrameBounds = 9; // DWMWA_EXTENDED_FRAME_BOUNDS
const DWORD kDwmCloaked = 14;            // DWMWA_CLOAKED

BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam)
{
    auto *out = reinterpret_cast<QVector<QRect> *>(lParam);

    if (!IsWindowVisible(hwnd) || IsIconic(hwnd))
        return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId())
        return TRUE;

    const LONG exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TRANSPARENT) // прозорі для миші оверлеї (лічильники FPS тощо)
        return TRUE;

    int cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, kDwmCloaked, &cloaked, sizeof(cloaked))) && cloaked)
        return TRUE; // приховані вікна UWP / інших віртуальних робочих столів

    RECT r;
    if (FAILED(DwmGetWindowAttribute(hwnd, kDwmExtendedFrameBounds, &r, sizeof(r)))) {
        if (!GetWindowRect(hwnd, &r))
            return TRUE;
    }
    const QRect q(QPoint(r.left, r.top), QPoint(r.right - 1, r.bottom - 1));
    if (q.width() < 8 || q.height() < 8)
        return TRUE;

    out->append(q);
    return TRUE;
}

} // namespace

QVector<QRect> windowRects()
{
    QVector<QRect> rects;
    EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&rects));
    return rects;
}

void forceForeground(QWidget *w)
{
    if (!w)
        return;
    HWND hwnd = reinterpret_cast<HWND>(w->winId());
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
}

#else

QVector<QRect> windowRects()
{
    return {};
}

void forceForeground(QWidget *w)
{
    Q_UNUSED(w)
}

#endif

} // namespace Platform
