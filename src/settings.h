#pragma once

#include <QColor>
#include <QString>
#include <QVariant>

class QSettings;

// Усі налаштування програми. Зберігаються в
// %APPDATA%\Lumio\Lumio.ini
// або поруч з .exe, якщо там лежить файл portable.txt (портативний режим).
namespace Settings {

QSettings &store();
bool isPortable();
QVariant value(const QString &key, const QVariant &def = QVariant());
void setValue(const QString &key, const QVariant &v);

QString defaultSaveDir();
QString saveDir();
QString imageFormat();      // "png" або "jpg"
int jpgQuality();
QString defaultFileNamePattern();
QString fileNamePattern();  // формат QDateTime, текст у 'лапках'

bool autoSaveOnCopy();      // при копіюванні також зберігати в галерею
bool copyOnSave();          // при збереженні також копіювати в буфер
bool showNotifications();
bool showMagnifier();
bool detectWindows();
bool confirmDelete();
bool useRecycleBin();

QString defaultHotkey(const QString &name);
QString hotkey(const QString &name); // "region", "region2", "fullscreen", "gallery"

QColor penColor();
int penWidth();

bool autostart();
void setAutostart(bool on);

} // namespace Settings
