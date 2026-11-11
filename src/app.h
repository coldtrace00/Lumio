#pragma once

#include "captureoverlay.h"

#include <QList>
#include <QObject>
#include <QPointer>
#include <QSystemTrayIcon>

class GalleryWindow;
class HotkeyManager;
class QLocalServer;
class QMenu;
class SettingsDialog;

// Головний контролер: значок у треї, гарячі клавіші, запуск захоплення,
// обробка дій (копіювати / зберегти / закріпити / друк), галерея та налаштування.
class App : public QObject
{
    Q_OBJECT
public:
    explicit App(QObject *parent = nullptr);
    ~App() override;

    static QString serverName();
    static bool sendToRunningInstance(const QString &command);

    void start(const QString &command);

public slots:
    void handleCommand(const QString &command);
    void captureRegion();
    void captureFullScreen();
    void captureDelayed(int seconds);
    void showGallery();
    void showSettings();
    void showAbout();
    void checkForUpdates();
    void openSaveFolder();

private:
    void setupTray();
    void registerHotkeys();
    void closeOverlays();
    void onOverlayAction(CaptureOverlay::Action action, const QImage &image, const QRect &globalRect);
    QString saveAndReport(const QImage &image, bool alsoCopy);
    void notify(const QString &title, const QString &text,
                QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information);
    void pin(const QImage &image, const QPoint &globalPos = QPoint());
    QString hotkeyText(const QString &name) const;

    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    HotkeyManager *m_hotkeys = nullptr;
    QLocalServer *m_server = nullptr;
    QList<QPointer<CaptureOverlay>> m_overlays;
    QPointer<GalleryWindow> m_gallery;
    QPointer<SettingsDialog> m_settings;
    QString m_lastSavedPath;
};
