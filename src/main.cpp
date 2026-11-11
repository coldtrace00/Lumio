// Lumio — відкритий скріншотер (альтернатива Lightshot) на C++/Qt.
// Ліцензія MIT.

#include "app.h"
#include "icons.h"

#include <QApplication>
#include <QThread>

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication a(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Lumio"));
    QApplication::setOrganizationName(QStringLiteral("Lumio"));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    QApplication::setQuitOnLastWindowClosed(false); // живемо в треї
    QApplication::setWindowIcon(Icons::appIcon());

    // Аргументи: --capture, --fullscreen, --gallery, --settings, --tray, --restart
    QString command;
    const QStringList args = QApplication::arguments();
    for (int i = 1; i < args.size(); ++i) {
        const QString arg = args.at(i);
        if (arg.startsWith(QLatin1String("--")))
            command = arg.mid(2).toLower();
    }

    // Перезапуск (наприклад, після зміни мови): дочекатися, поки стара копія закриється
    if (command == QLatin1String("restart")) {
        for (int i = 0; i < 60 && App::sendToRunningInstance(QStringLiteral("ping")); ++i)
            QThread::msleep(100);
        command = QStringLiteral("tray");
    }

    // Якщо програма вже запущена — передати їй команду і вийти
    if (App::sendToRunningInstance(command.isEmpty() ? QStringLiteral("show") : command))
        return 0;

    App app;
    app.start(command);
    return QApplication::exec();
}
