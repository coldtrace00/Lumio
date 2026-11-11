#include "settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>

namespace Settings {

#ifdef Q_OS_WIN
namespace {
const char *kRunKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const char *kRunName = "Lumio";
}
#endif

QSettings &store()
{
    static QSettings *s = nullptr;
    if (!s) {
        const QString appDir = QCoreApplication::applicationDirPath();
        if (isPortable()) {
            s = new QSettings(appDir + QStringLiteral("/Lumio.ini"), QSettings::IniFormat);
        } else {
            s = new QSettings(QSettings::IniFormat, QSettings::UserScope,
                              QStringLiteral("Lumio"), QStringLiteral("Lumio"));
        }
    }
    return *s;
}

QVariant value(const QString &key, const QVariant &def)
{
    return store().value(key, def);
}

void setValue(const QString &key, const QVariant &v)
{
    store().setValue(key, v);
}

bool isPortable()
{
    return QFile::exists(QCoreApplication::applicationDirPath() + QStringLiteral("/portable.txt"));
}

QString defaultSaveDir()
{
    // Портативна версія тримає знімки поруч із собою
    if (isPortable())
        return QCoreApplication::applicationDirPath() + QStringLiteral("/Screenshots");
    QString base = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (base.isEmpty())
        base = QDir::homePath();
    return QDir::cleanPath(base + QStringLiteral("/Lumio"));
}

QString saveDir()
{
    const QString d = value(QStringLiteral("general/saveDir")).toString().trimmed();
    return d.isEmpty() ? defaultSaveDir() : d;
}

QString imageFormat()
{
    const QString f = value(QStringLiteral("general/format"), QStringLiteral("png")).toString().toLower();
    return (f == QLatin1String("jpg") || f == QLatin1String("jpeg")) ? QStringLiteral("jpg") : QStringLiteral("png");
}

int jpgQuality()
{
    return qBound(1, value(QStringLiteral("general/jpgQuality"), 92).toInt(), 100);
}

QString defaultFileNamePattern()
{
    return QStringLiteral("'Screenshot_'yyyy-MM-dd_HH-mm-ss");
}

QString fileNamePattern()
{
    const QString p = value(QStringLiteral("general/fileNamePattern")).toString();
    return p.trimmed().isEmpty() ? defaultFileNamePattern() : p;
}

bool autoSaveOnCopy()    { return value(QStringLiteral("capture/autoSaveOnCopy"), true).toBool(); }
bool copyOnSave()        { return value(QStringLiteral("capture/copyOnSave"), false).toBool(); }
bool showNotifications() { return value(QStringLiteral("capture/showNotifications"), true).toBool(); }
bool showMagnifier()     { return value(QStringLiteral("capture/showMagnifier"), true).toBool(); }
bool detectWindows()     { return value(QStringLiteral("capture/detectWindows"), true).toBool(); }
bool confirmDelete()     { return value(QStringLiteral("gallery/confirmDelete"), true).toBool(); }
bool useRecycleBin()     { return value(QStringLiteral("gallery/useRecycleBin"), true).toBool(); }

QString defaultHotkey(const QString &name)
{
    if (name == QLatin1String("region"))
        return QStringLiteral("Print");
    if (name == QLatin1String("region2"))
        return QStringLiteral("Meta+Shift+S"); // кнопка скріншота Logitech MX Keys / Win+Shift+S
    if (name == QLatin1String("fullscreen"))
        return QStringLiteral("Shift+Print");
    if (name == QLatin1String("gallery"))
        return QStringLiteral("Ctrl+Print");
    return QString();
}

QString hotkey(const QString &name)
{
    return value(QStringLiteral("hotkeys/") + name, defaultHotkey(name)).toString();
}

QColor penColor()
{
    const QColor c(value(QStringLiteral("editor/color"), QStringLiteral("#ffff3b30")).toString());
    return c.isValid() ? c : QColor(255, 59, 48);
}

int penWidth()
{
    return qBound(1, value(QStringLiteral("editor/width"), 3).toInt(), 30);
}

bool autostart()
{
#ifdef Q_OS_WIN
    QSettings reg(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
    return reg.contains(QString::fromLatin1(kRunName));
#else
    return value(QStringLiteral("general/autostart"), false).toBool();
#endif
}

void setAutostart(bool on)
{
#ifdef Q_OS_WIN
    QSettings reg(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
    if (on) {
        const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        reg.setValue(QString::fromLatin1(kRunName), QStringLiteral("\"%1\" --tray").arg(exe));
    } else {
        reg.remove(QString::fromLatin1(kRunName));
    }
#else
    setValue(QStringLiteral("general/autostart"), on);
#endif
}

} // namespace Settings
