#include "app.h"
#include "i18n.h"

#include "gallerywindow.h"
#include "hotkeymanager.h"
#include "icons.h"
#include "imageio.h"
#include "pinwindow.h"
#include "platform.h"
#include "settings.h"
#include "settingsdialog.h"

#include <QApplication>
#include <QCursor>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMenu>
#include "updater.h"
#include <QMessageBox>
#include <QPixmap>
#include <QProcess>
#include <QScreen>
#include <QTimer>

#include <cmath>
#include <utility>

namespace {
enum HotkeyId { HkRegion = 1, HkFullscreen = 2, HkGallery = 3 };
}

App::App(QObject *parent)
    : QObject(parent)
{
}

App::~App()
{
    closeOverlays();
    delete m_menu;
}

QString App::serverName()
{
    QString user = qEnvironmentVariable("USERNAME");
    if (user.isEmpty())
        user = qEnvironmentVariable("USER");
    return QStringLiteral("Lumio-") + user;
}

bool App::sendToRunningInstance(const QString &command)
{
    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (!socket.waitForConnected(500))
        return false;
    socket.write(command.toUtf8());
    socket.flush();
    socket.waitForBytesWritten(1000);
    socket.disconnectFromServer();
    if (socket.state() != QLocalSocket::UnconnectedState)
        socket.waitForDisconnected(500);
    return true;
}

void App::start(const QString &command)
{
    // Сервер для команд від повторного запуску (Lumio.exe --capture тощо)
    m_server = new QLocalServer(this);
    QLocalServer::removeServer(serverName());
    m_server->listen(serverName());
    connect(m_server, &QLocalServer::newConnection, this, [this]() {
        while (QLocalSocket *s = m_server->nextPendingConnection()) {
            connect(s, &QLocalSocket::readyRead, this, [this, s]() {
                handleCommand(QString::fromUtf8(s->readAll()).trimmed());
            });
            connect(s, &QLocalSocket::disconnected, s, &QObject::deleteLater);
        }
    });

    m_hotkeys = new HotkeyManager(this);
    connect(m_hotkeys, &HotkeyManager::activated, this, [this](int id) {
        switch (id) {
        case HkRegion: captureRegion(); break;
        case HkFullscreen: captureFullScreen(); break;
        case HkGallery: showGallery(); break;
        default: break;
        }
    });

    setupTray();
    registerHotkeys();

    if (command.isEmpty()) {
        notify(tr2("Lumio працює", "Lumio is running"),
               tr2("Натисніть %1 або клацніть по значку в треї, щоб зробити скріншот.", "Press %1 or click the tray icon to take a screenshot.")
                   .arg(hotkeyText(QStringLiteral("region"))));
    } else {
        handleCommand(command);
    }
}

void App::handleCommand(const QString &command)
{
    if (command == QLatin1String("capture"))
        QTimer::singleShot(0, this, &App::captureRegion);
    else if (command == QLatin1String("fullscreen"))
        QTimer::singleShot(0, this, &App::captureFullScreen);
    else if (command == QLatin1String("gallery") || command == QLatin1String("show"))
        showGallery();
    else if (command == QLatin1String("settings"))
        showSettings();
    // "tray" — нічого не робити (тихий запуск з автозавантаження)
}

QString App::hotkeyText(const QString &name) const
{
    return HotkeyManager::displayText(Settings::hotkey(name));
}

// ---------------------------------------------------------------------------
// Трей та гарячі клавіші

void App::setupTray()
{
    m_menu = new QMenu;

    m_menu->addAction(Icons::themed(Icons::Capture), tr2("Зробити скріншот", "Take screenshot"), this, &App::captureRegion);
    m_menu->addAction(Icons::themed(Icons::Fullscreen), tr2("Весь екран", "Full screen"), this, &App::captureFullScreen);
    QMenu *delay = m_menu->addMenu(Icons::themed(Icons::Timer), tr2("Із затримкою", "With delay"));
    for (int s : {3, 5, 10})
        delay->addAction(tr2("%1 с", "%1 s").arg(s), this, [this, s]() { captureDelayed(s); });
    m_menu->addSeparator();
    m_menu->addAction(Icons::themed(Icons::Gallery), tr2("Галерея знімків", "Screenshot gallery"), this, &App::showGallery);
    m_menu->addAction(Icons::themed(Icons::Folder), tr2("Відкрити папку знімків", "Open screenshots folder"), this, &App::openSaveFolder);
    m_menu->addSeparator();
    m_menu->addAction(Icons::themed(Icons::Gear), tr2("Налаштування", "Settings"), this, &App::showSettings);
    m_menu->addAction(Icons::themed(Icons::Refresh), tr2("Перевірити й оновити…", "Check for updates…"), this, &App::checkForUpdates);
    m_menu->addAction(Icons::themed(Icons::Info), tr2("Про програму", "About"), this, &App::showAbout);
    m_menu->addSeparator();
    m_menu->addAction(Icons::themed(Icons::Exit), tr2("Вихід", "Exit"), qApp, &QCoreApplication::quit);

    m_tray = new QSystemTrayIcon(Icons::appIcon(), this);
    m_tray->setContextMenu(m_menu);
    m_tray->setToolTip(tr2("Lumio — скріншоти", "Lumio — screenshots"));
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger)
            captureRegion();
        else if (r == QSystemTrayIcon::MiddleClick)
            showGallery();
    });
    connect(m_tray, &QSystemTrayIcon::messageClicked, this, [this]() {
        showGallery();
        if (m_gallery && !m_lastSavedPath.isEmpty())
            m_gallery->selectPath(m_lastSavedPath);
    });
    m_tray->show();

    if (!QSystemTrayIcon::isSystemTrayAvailable())
        QTimer::singleShot(0, this, &App::showGallery); // без трею — показати хоч щось
}

void App::registerHotkeys()
{
    m_hotkeys->clear();
    const struct { int id; const char *name; } list[] = {
        {HkRegion, "region"}, {HkRegion, "region2"}, {HkFullscreen, "fullscreen"}, {HkGallery, "gallery"},
    };
    QStringList failed;
    for (const auto &h : list) {
        const QString s = Settings::hotkey(QString::fromLatin1(h.name));
        if (s.isEmpty())
            continue;
        if (!m_hotkeys->addHotkey(h.id, s))
            failed << s;
    }
#ifdef Q_OS_WIN
    if (!m_hotkeys->isActive()) {
        notify(tr2("Гарячі клавіші не працюють", "Hotkeys are not working"),
               tr2("Не вдалося перехопити клавіатуру. Знімок можна зробити кліком по значку в треї.", "Could not hook the keyboard. You can still take a screenshot by clicking the tray icon."),
               QSystemTrayIcon::Warning);
    } else if (!failed.isEmpty()) {
        notify(tr2("Невідома клавіша", "Unknown key"),
               tr2("Не вдалося розпізнати: %1. Перепризначте її в налаштуваннях.", "Could not recognize: %1. Reassign it in the settings.")
                   .arg(failed.join(QStringLiteral(", "))),
               QSystemTrayIcon::Warning);
    }
#else
    Q_UNUSED(failed)
#endif
    QString tip = tr2("Lumio — скріншоти", "Lumio — screenshots");
    QStringList keys;
    for (const char *n : {"region", "region2"}) {
        const QString t = hotkeyText(QString::fromLatin1(n));
        if (!t.isEmpty())
            keys << t;
    }
    if (!keys.isEmpty())
        tip += tr2("\n%1 — виділити область", "\n%1 — select area").arg(keys.join(tr2(" або ", " or ")));
    if (m_tray)
        m_tray->setToolTip(tip);
}

void App::notify(const QString &title, const QString &text, QSystemTrayIcon::MessageIcon icon)
{
    if (!m_tray || !Settings::showNotifications() || !QSystemTrayIcon::supportsMessages())
        return;
    m_tray->showMessage(title, text, icon, 3000);
}

// ---------------------------------------------------------------------------
// Захоплення

void App::captureRegion()
{
    if (!m_overlays.isEmpty()) {
        for (const auto &ov : std::as_const(m_overlays))
            if (ov)
                ov->activate();
        return;
    }

    // 1) Спершу знімаємо всі екрани (поки нічого нашого на них немає)
    const QVector<QRect> nativeWindows = Settings::detectWindows() ? Platform::windowRects() : QVector<QRect>();
    struct Shot {
        QScreen *screen;
        QPixmap pixmap;
    };
    QList<Shot> shots;
    for (QScreen *s : QGuiApplication::screens()) {
        QPixmap pm = s->grabWindow(0);
        if (pm.isNull())
            continue;
        const QRect g = s->geometry();
        qreal dpr = g.width() > 0 ? qreal(pm.width()) / g.width() : 1.0;
        if (dpr <= 0)
            dpr = 1.0;
        pm.setDevicePixelRatio(dpr);
        shots.append({s, pm});
    }
    if (shots.isEmpty()) {
        notify(tr2("Помилка", "Error"), tr2("Не вдалося зробити знімок екрана", "Could not capture the screen"), QSystemTrayIcon::Warning);
        return;
    }

    // 2) Потім показуємо оверлей на кожному моніторі
    QScreen *cursorScreen = QGuiApplication::screenAt(QCursor::pos());
    CaptureOverlay *focusOverlay = nullptr;
    for (const Shot &sh : std::as_const(shots)) {
        const QRect g = sh.screen->geometry();
        const qreal dpr = sh.pixmap.devicePixelRatio();
        QVector<QRect> local;
        const QRect bounds(QPoint(0, 0), g.size());
        for (const QRect &w : nativeWindows) {
            // Qt зберігає верхній лівий кут екрана в нативних координатах, а розмір — у логічних
            const QRect r(QPoint(int(std::floor((w.left() - g.left()) / dpr)), int(std::floor((w.top() - g.top()) / dpr))),
                          QSize(int(std::ceil(w.width() / dpr)), int(std::ceil(w.height() / dpr))));
            const QRect clipped = r.intersected(bounds);
            if (!clipped.isEmpty())
                local.append(clipped);
        }

        auto *ov = new CaptureOverlay(sh.screen, sh.pixmap, local);
        connect(ov, &CaptureOverlay::actionRequested, this, &App::onOverlayAction);
        connect(ov, &CaptureOverlay::cancelled, this, &App::closeOverlays);
        m_overlays.append(ov);
        ov->showOnScreen();
        if (sh.screen == cursorScreen || !focusOverlay)
            focusOverlay = ov;
    }
    if (focusOverlay)
        focusOverlay->activate();
}

void App::captureFullScreen()
{
    QScreen *s = QGuiApplication::screenAt(QCursor::pos());
    if (!s)
        s = QGuiApplication::primaryScreen();
    if (!s)
        return;
    const QPixmap pm = s->grabWindow(0);
    if (pm.isNull()) {
        notify(tr2("Помилка", "Error"), tr2("Не вдалося зробити знімок екрана", "Could not capture the screen"), QSystemTrayIcon::Warning);
        return;
    }
    saveAndReport(pm.toImage(), true);
}

void App::captureDelayed(int seconds)
{
    QTimer::singleShot(seconds * 1000, this, &App::captureRegion);
}

void App::closeOverlays()
{
    const auto overlays = m_overlays;
    m_overlays.clear();
    for (const auto &ov : overlays) {
        if (ov) {
            ov->hide();
            ov->deleteLater();
        }
    }
}

QString App::saveAndReport(const QImage &image, bool alsoCopy)
{
    QString err;
    const QString path = ImageIO::saveToGallery(image, &err);
    if (path.isEmpty()) {
        notify(tr2("Не вдалося зберегти", "Could not save"), err, QSystemTrayIcon::Warning);
        return QString();
    }
    if (alsoCopy)
        ImageIO::copyToClipboard(image);
    m_lastSavedPath = path;
    notify(tr2("Знімок збережено", "Screenshot saved"),
           QFileInfo(path).fileName() + (alsoCopy ? tr2("\nТакож скопійовано в буфер обміну", "\nAlso copied to the clipboard") : QString()));
    return path;
}

void App::onOverlayAction(CaptureOverlay::Action action, const QImage &image, const QRect &globalRect)
{
    closeOverlays();
    const QImage img = image;

    switch (action) {
    case CaptureOverlay::Action::Copy: {
        ImageIO::copyToClipboard(img);
        QString text = tr2("Вставте його будь-де через Ctrl+V", "Paste it anywhere with Ctrl+V");
        if (Settings::autoSaveOnCopy()) {
            QString err;
            const QString path = ImageIO::saveToGallery(img, &err);
            if (!path.isEmpty()) {
                m_lastSavedPath = path;
                text = tr2("Також збережено в галерею: %1", "Also saved to the gallery: %1").arg(QFileInfo(path).fileName());
            }
        }
        notify(tr2("Скопійовано в буфер обміну", "Copied to the clipboard"), text);
        break;
    }
    case CaptureOverlay::Action::Save:
        saveAndReport(img, Settings::copyOnSave());
        break;
    case CaptureOverlay::Action::SaveAs:
        QTimer::singleShot(0, this, [this, img]() {
            const QString path = ImageIO::saveAsDialog(nullptr, img);
            if (!path.isEmpty())
                notify(tr2("Знімок збережено", "Screenshot saved"), QDir::toNativeSeparators(path));
        });
        break;
    case CaptureOverlay::Action::Pin:
        pin(img, globalRect.topLeft());
        break;
    case CaptureOverlay::Action::Print:
        QTimer::singleShot(0, this, [img]() { ImageIO::printImage(nullptr, img); });
        break;
    }
}

void App::pin(const QImage &image, const QPoint &globalPos)
{
    auto *w = new PinWindow(image, globalPos);
    connect(w, &PinWindow::savedToGallery, this, [this](const QString &path) {
        m_lastSavedPath = path;
        notify(tr2("Знімок збережено", "Screenshot saved"), QFileInfo(path).fileName());
    });
    w->show();
    w->raise();
    w->activateWindow();
}

// ---------------------------------------------------------------------------
// Вікна

void App::showGallery()
{
    if (!m_gallery) {
        m_gallery = new GalleryWindow;
        connect(m_gallery, &GalleryWindow::captureRequested, this, [this]() {
            if (m_gallery)
                m_gallery->showMinimized();
            QTimer::singleShot(350, this, &App::captureRegion);
        });
        connect(m_gallery, &GalleryWindow::settingsRequested, this, &App::showSettings);
        connect(m_gallery, &GalleryWindow::pinRequested, this, [this](const QImage &img) { pin(img); });
    }
    if (m_gallery->isMinimized())
        m_gallery->showNormal();
    m_gallery->show();
    m_gallery->raise();
    m_gallery->activateWindow();
    Platform::forceForeground(m_gallery);
}

void App::showSettings()
{
    if (m_settings) {
        m_settings->raise();
        m_settings->activateWindow();
        return;
    }
    m_settings = new SettingsDialog(m_hotkeys, m_gallery);
    m_settings->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_settings, &QDialog::accepted, this, [this]() {
        registerHotkeys();
        if (m_gallery)
            m_gallery->reload();
        if (m_settings && m_settings->languageChanged()) {
            const auto answer = QMessageBox::question(
                nullptr, QStringLiteral("Lumio"),
                QStringLiteral("Мову змінено. Перезапустити Lumio зараз?\n\nLanguage changed. Restart Lumio now?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            if (answer == QMessageBox::Yes) {
                QProcess::startDetached(QCoreApplication::applicationFilePath(), {QStringLiteral("--restart")});
                qApp->quit();
            }
        }
    });
    m_settings->show();
    m_settings->raise();
    m_settings->activateWindow();
    Platform::forceForeground(m_settings);
}

void App::openSaveFolder()
{
    ImageIO::openFolder(Settings::saveDir());
}

void App::checkForUpdates()
{
    auto *dlg = new UpdateDialog({QStringLiteral("Lumio"), QStringLiteral("coldtrace00/Lumio"), QStringLiteral("Lumio-Setup.exe"),
                                  QCoreApplication::applicationVersion()});
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowIcon(Icons::appIcon());
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void App::showAbout()
{
    const QString text = tr2("<h3>Lumio %1</h3>"
        "<p>Відкритий скріншотер для Windows на C++/Qt — альтернатива Lightshot.</p>"
        "<p><b>Гарячі клавіші:</b><br>"
        "%2 — виділити область<br>"
        "%3 — весь екран одразу<br>"
        "%4 — галерея</p>"
        "<p><b>У режимі виділення:</b><br>"
        "Ctrl+C / Enter / подвійний клік — копіювати<br>"
        "Ctrl+S — зберегти в галерею, Ctrl+Shift+S — зберегти як<br>"
        "Ctrl+T — закріпити на екрані, Ctrl+P — друк<br>"
        "Ctrl+Z / Ctrl+Y — скасувати / повторити<br>"
        "P L A R E M T B N — інструменти, колесо — товщина<br>"
        "Стрілки — посунути виділення, Alt+стрілки — змінити розмір</p>"
        "<p>Ліцензія MIT. Qt %5.<br><a href=\"https://github.com/coldtrace00/Lumio\">github.com/coldtrace00/Lumio</a></p>", "<h3>Lumio %1</h3><p>An open-source screenshot tool for Windows in C++/Qt — an alternative to Lightshot.</p><p><b>Hotkeys:</b><br>%2 — select area<br>%3 — full screen at once<br>%4 — gallery</p><p><b>While selecting:</b><br>Ctrl+C / Enter / double-click — copy<br>Ctrl+S — save to gallery, Ctrl+Shift+S — save as<br>Ctrl+T — pin to screen, Ctrl+P — print<br>Ctrl+Z / Ctrl+Y — undo / redo<br>P L A R E M T B N — tools, mouse wheel — thickness<br>Arrows — move the selection, Alt+arrows — resize</p><p>MIT license. Qt %5.<br><a href=\"https://github.com/coldtrace00/Lumio\">github.com/coldtrace00/Lumio</a></p>")
        .arg(QCoreApplication::applicationVersion(),
             hotkeyText(QStringLiteral("region")).isEmpty() ? QStringLiteral("—") : hotkeyText(QStringLiteral("region")),
             hotkeyText(QStringLiteral("fullscreen")).isEmpty() ? QStringLiteral("—") : hotkeyText(QStringLiteral("fullscreen")),
             hotkeyText(QStringLiteral("gallery")).isEmpty() ? QStringLiteral("—") : hotkeyText(QStringLiteral("gallery")),
             QString::fromLatin1(qVersion()));
    QMessageBox box;
    box.setWindowTitle(tr2("Про Lumio", "About Lumio"));
    box.setWindowIcon(Icons::appIcon());
    box.setIconPixmap(Icons::appPixmap(64));
    box.setTextFormat(Qt::RichText);
    box.setText(text);
    box.exec();
}
