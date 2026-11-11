#include "hotkeymanager.h"
#include "i18n.h"

#include <QKeySequence>
#include <QMetaObject>
#include <QMutexLocker>
#include <QSemaphore>
#include <QStringList>
#include <QThread>

#include <utility>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

// Віртуальні коди клавіш Windows (числами, щоб файл збирався і на інших ОС)
struct KeyMap {
    Qt::Key key;
    unsigned vk;
};

const KeyMap kKeyMap[] = {
    {Qt::Key_Print, 0x2C},     {Qt::Key_Pause, 0x13},   {Qt::Key_ScrollLock, 0x91},
    {Qt::Key_Insert, 0x2D},    {Qt::Key_Delete, 0x2E},  {Qt::Key_Home, 0x24},
    {Qt::Key_End, 0x23},       {Qt::Key_PageUp, 0x21},  {Qt::Key_PageDown, 0x22},
    {Qt::Key_Space, 0x20},     {Qt::Key_Tab, 0x09},     {Qt::Key_Escape, 0x1B},
    {Qt::Key_Backspace, 0x08}, {Qt::Key_Return, 0x0D},  {Qt::Key_Left, 0x25},
    {Qt::Key_Up, 0x26},        {Qt::Key_Right, 0x27},   {Qt::Key_Down, 0x28},
    {Qt::Key_Calculator, 0xB7},
};

bool isModifierVk(unsigned vk)
{
    switch (vk) {
    case 0x10: case 0xA0: case 0xA1: // Shift
    case 0x11: case 0xA2: case 0xA3: // Ctrl
    case 0x12: case 0xA4: case 0xA5: // Alt
    case 0x5B: case 0x5C:            // Win
        return true;
    default:
        return false;
    }
}

unsigned qtKeyToVk(Qt::Key key)
{
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        return 0x70 + (key - Qt::Key_F1);
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return 0x41 + (key - Qt::Key_A);
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return 0x30 + (key - Qt::Key_0);
    if (key == Qt::Key_Enter)
        return 0x0D;
    for (const KeyMap &m : kKeyMap)
        if (m.key == key)
            return m.vk;
    return 0;
}

Qt::Key vkToQtKey(unsigned vk)
{
    if (vk >= 0x70 && vk <= 0x87)
        return Qt::Key(Qt::Key_F1 + (vk - 0x70));
    if (vk >= 0x41 && vk <= 0x5A)
        return Qt::Key(Qt::Key_A + (vk - 0x41));
    if (vk >= 0x30 && vk <= 0x39)
        return Qt::Key(Qt::Key_0 + (vk - 0x30));
    for (const KeyMap &m : kKeyMap)
        if (m.vk == vk)
            return m.key;
    return Qt::Key_unknown;
}

} // namespace

// ---------------------------------------------------------------------------
// Розбір / складання тексту комбінації

QString HotkeyManager::keyName(unsigned vk)
{
    const Qt::Key k = vkToQtKey(vk);
    if (k != Qt::Key_unknown)
        return QKeySequence(int(k)).toString(QKeySequence::PortableText);
    return QStringLiteral("VK%1").arg(vk);
}

QString HotkeyManager::compose(unsigned vk, unsigned mods)
{
    QStringList parts;
    if (mods & ModCtrl)  parts << QStringLiteral("Ctrl");
    if (mods & ModAlt)   parts << QStringLiteral("Alt");
    if (mods & ModShift) parts << QStringLiteral("Shift");
    if (mods & ModWin)   parts << QStringLiteral("Meta");
    parts << keyName(vk);
    return parts.join(QLatin1Char('+'));
}

bool HotkeyManager::parse(const QString &combo, unsigned &vk, unsigned &mods)
{
    vk = 0;
    mods = 0;
    const QStringList parts = combo.trimmed().split(QLatin1Char('+'), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return false;
    for (int i = 0; i < parts.size() - 1; ++i) {
        const QString p = parts.at(i).trimmed().toLower();
        if (p == QLatin1String("ctrl") || p == QLatin1String("control"))
            mods |= ModCtrl;
        else if (p == QLatin1String("alt"))
            mods |= ModAlt;
        else if (p == QLatin1String("shift"))
            mods |= ModShift;
        else if (p == QLatin1String("meta") || p == QLatin1String("win"))
            mods |= ModWin;
        else
            return false;
    }
    const QString key = parts.last().trimmed();
    if (key.startsWith(QLatin1String("VK"), Qt::CaseInsensitive)) {
        bool ok = false;
        vk = key.mid(2).toUInt(&ok, 0);
        return ok && vk > 0 && vk < 256;
    }
    const QKeySequence seq = QKeySequence::fromString(key, QKeySequence::PortableText);
    if (seq.isEmpty())
        return false;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    vk = qtKeyToVk(seq[0].key());
#else
    vk = qtKeyToVk(Qt::Key(seq[0] & ~Qt::KeyboardModifierMask));
#endif
    return vk != 0;
}

QString HotkeyManager::displayText(const QString &combo)
{
    unsigned vk = 0, mods = 0;
    if (!parse(combo, vk, mods))
        return QString();
    QStringList parts;
    if (mods & ModWin)   parts << QStringLiteral("Win");
    if (mods & ModCtrl)  parts << QStringLiteral("Ctrl");
    if (mods & ModAlt)   parts << QStringLiteral("Alt");
    if (mods & ModShift) parts << QStringLiteral("Shift");
    QString k = keyName(vk);
    if (vk == 0x2C)
        k = QStringLiteral("PrtSc");
    else if (k.startsWith(QLatin1String("VK")))
        k = tr2("Клавіша 0x%1", "Key 0x%1").arg(QString::number(vk, 16).toUpper());
    parts << k;
    return parts.join(QLatin1Char('+'));
}

// ---------------------------------------------------------------------------
// Обробка натискань (викликається з потоку хука)

bool HotkeyManager::processKey(unsigned vk, bool down, bool up, unsigned mods)
{
    if (isModifierVk(vk))
        return false;

    QMutexLocker lock(&m_mutex);

    if (up) {
        // Відпускання клавіші, натискання якої ми проковтнули, — теж ховаємо
        return m_swallowUp.remove(vk);
    }
    if (!down)
        return false;

    if (m_recording) {
        m_recording = false;
        m_swallowUp.insert(vk);
        const QString combo = (vk == 0x1B && mods == 0) ? QString() : compose(vk, mods);
        QMetaObject::invokeMethod(this, [this, combo]() { emit recorded(combo); }, Qt::QueuedConnection);
        return true;
    }

    for (const Combo &c : std::as_const(m_combos)) {
        if (c.vk != vk || c.mods != mods)
            continue;
        if (m_swallowUp.contains(vk))
            return true; // автоповтор утримуваної клавіші — не запускати вдруге
        m_swallowUp.insert(vk);
        const int id = c.id;
        QMetaObject::invokeMethod(this, [this, id]() { emit activated(id); }, Qt::QueuedConnection);
        return true;
    }
    return false;
}

void HotkeyManager::clear()
{
    QMutexLocker lock(&m_mutex);
    m_combos.clear();
}

bool HotkeyManager::addHotkey(int id, const QString &combo)
{
    unsigned vk = 0, mods = 0;
    if (!parse(combo, vk, mods))
        return false;
    QMutexLocker lock(&m_mutex);
    m_combos.append({id, vk, mods});
    return true;
}

void HotkeyManager::startRecording()
{
    QMutexLocker lock(&m_mutex);
    m_recording = true;
}

void HotkeyManager::cancelRecording()
{
    QMutexLocker lock(&m_mutex);
    m_recording = false;
}

// ---------------------------------------------------------------------------
// Windows: хук клавіатури в окремому потоці

#ifdef Q_OS_WIN
namespace {

HotkeyManager *g_manager = nullptr;
HHOOK g_hook = nullptr;
const ULONG_PTR kOwnInputMarker = 0x41534331; // позначка наших власних SendInput

unsigned currentModifiers()
{
    unsigned m = 0;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) m |= HotkeyManager::ModCtrl;
    if (GetAsyncKeyState(VK_MENU) & 0x8000)    m |= HotkeyManager::ModAlt;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000)   m |= HotkeyManager::ModShift;
    if ((GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000))
        m |= HotkeyManager::ModWin;
    return m;
}

// Якщо проковтнути клавішу в комбінації з Win, то при відпусканні Win відкриється меню «Пуск».
// Невикористана клавіша 0xE8 між ними цьому запобігає (так само робить PowerToys).
void sendDummyKey()
{
    INPUT in[2] = {};
    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wVk = 0xE8;
    in[0].ki.dwExtraInfo = kOwnInputMarker;
    in[1] = in[0];
    in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, in, sizeof(INPUT));
}

LRESULT CALLBACK lowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && g_manager) {
        const auto *k = reinterpret_cast<const KBDLLHOOKSTRUCT *>(lParam);
        if (k->dwExtraInfo != kOwnInputMarker) {
            const bool down = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
            const bool up = wParam == WM_KEYUP || wParam == WM_SYSKEYUP;
            const unsigned mods = down ? currentModifiers() : 0;
            if (g_manager->processKey(k->vkCode, down, up, mods)) {
                if (down && (mods & HotkeyManager::ModWin))
                    sendDummyKey();
                return 1;
            }
        }
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

class HookThread : public QThread
{
public:
    QSemaphore ready;
    bool ok = false;
    DWORD threadId = 0;

    void stop()
    {
        if (threadId)
            PostThreadMessageW(threadId, WM_QUIT, 0, 0);
        wait(3000);
    }

protected:
    void run() override
    {
        threadId = GetCurrentThreadId();
        MSG msg;
        PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE); // створити чергу повідомлень
        g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, lowLevelKeyboardProc, GetModuleHandleW(nullptr), 0);
        ok = g_hook != nullptr;
        ready.release();
        if (!g_hook)
            return;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        UnhookWindowsHookEx(g_hook);
        g_hook = nullptr;
    }
};

} // namespace
#endif

HotkeyManager::HotkeyManager(QObject *parent)
    : QObject(parent)
{
#ifdef Q_OS_WIN
    g_manager = this;
    auto *t = new HookThread;
    t->setObjectName(QStringLiteral("KeyboardHook"));
    m_thread.reset(t);
    t->start(QThread::TimeCriticalPriority);
    t->ready.acquire();
#endif
}

HotkeyManager::~HotkeyManager()
{
#ifdef Q_OS_WIN
    if (m_thread)
        static_cast<HookThread *>(m_thread.get())->stop();
    m_thread.reset();
    g_manager = nullptr;
#endif
}

bool HotkeyManager::isActive() const
{
#ifdef Q_OS_WIN
    return m_thread && static_cast<HookThread *>(m_thread.get())->ok;
#else
    return false;
#endif
}
