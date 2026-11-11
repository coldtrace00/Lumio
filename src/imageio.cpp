#include "imageio.h"
#include "i18n.h"
#include "settings.h"

#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageWriter>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QProcess>
#include <QUrl>

#include <QtPrintSupport/qtprintsupportglobal.h>
#if QT_CONFIG(printdialog)
#include <QPrintDialog>
#include <QPrinter>
#endif

namespace ImageIO {

QStringList nameFilters()
{
    return {QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.bmp")};
}

static QString sanitizeFileName(QString name)
{
    static const QString bad = QStringLiteral("<>:\"/\\|?*");
    for (QChar &c : name) {
        if (bad.contains(c) || c.unicode() < 32)
            c = QLatin1Char('_');
    }
    name = name.trimmed();
    while (name.endsWith(QLatin1Char('.')))
        name.chop(1);
    return name.isEmpty() ? QStringLiteral("Screenshot") : name;
}

QString nextGalleryPath()
{
    QDir dir(Settings::saveDir());
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));

    const QString base = sanitizeFileName(QDateTime::currentDateTime().toString(Settings::fileNamePattern()));
    const QString ext = Settings::imageFormat();
    QString path = dir.filePath(base + QLatin1Char('.') + ext);
    int i = 1;
    while (QFile::exists(path))
        path = dir.filePath(QStringLiteral("%1_%2.%3").arg(base).arg(i++).arg(ext));
    return path;
}

bool saveImage(const QImage &img, const QString &path, QString *error)
{
    if (img.isNull()) {
        if (error)
            *error = tr2("Порожнє зображення", "Empty image");
        return false;
    }
    const QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());

    const QString suffix = fi.suffix().toLower();
    QByteArray fmt = "PNG";
    if (suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg"))
        fmt = "JPEG";
    else if (suffix == QLatin1String("bmp"))
        fmt = "BMP";

    QImage out = img;
    out.setDevicePixelRatio(1.0);

    QImageWriter writer(path, fmt);
    if (fmt == "JPEG") {
        writer.setQuality(Settings::jpgQuality());
        out = out.convertToFormat(QImage::Format_RGB888);
    } else if (fmt == "BMP") {
        out = out.convertToFormat(QImage::Format_RGB32);
    }
    if (!writer.write(out)) {
        if (error)
            *error = writer.errorString();
        return false;
    }
    return true;
}

QString saveToGallery(const QImage &img, QString *error)
{
    const QString path = nextGalleryPath();
    return saveImage(img, path, error) ? path : QString();
}

void copyToClipboard(const QImage &img)
{
    QImage c = img;
    c.setDevicePixelRatio(1.0);
    QGuiApplication::clipboard()->setImage(c);
}

void copyFilesToClipboard(const QStringList &paths, const QImage &previewImage)
{
    auto *md = new QMimeData;
    QList<QUrl> urls;
    for (const QString &p : paths)
        urls << QUrl::fromLocalFile(p);
    md->setUrls(urls);
    if (!previewImage.isNull())
        md->setImageData(previewImage);
    QGuiApplication::clipboard()->setMimeData(md);
}

QString saveAsDialog(QWidget *parent, const QImage &img)
{
    const QString pngFilter = QStringLiteral("PNG (*.png)");
    const QString jpgFilter = QStringLiteral("JPEG (*.jpg *.jpeg)");
    const QString bmpFilter = QStringLiteral("BMP (*.bmp)");
    QString selected = Settings::imageFormat() == QLatin1String("jpg") ? jpgFilter : pngFilter;

    const QString lastDir = Settings::value(QStringLiteral("general/lastSaveAsDir"), Settings::saveDir()).toString();
    const QString suggested = QFileInfo(nextGalleryPath()).fileName();

    QString path = QFileDialog::getSaveFileName(parent, tr2("Зберегти знімок як", "Save screenshot as"),
                                                QDir(lastDir).filePath(suggested),
                                                pngFilter + QStringLiteral(";;") + jpgFilter + QStringLiteral(";;") + bmpFilter,
                                                &selected);
    if (path.isEmpty())
        return QString();

    if (QFileInfo(path).suffix().isEmpty()) {
        if (selected == jpgFilter)
            path += QStringLiteral(".jpg");
        else if (selected == bmpFilter)
            path += QStringLiteral(".bmp");
        else
            path += QStringLiteral(".png");
    }

    QString err;
    if (!saveImage(img, path, &err)) {
        QMessageBox::warning(parent, tr2("Помилка", "Error"),
                             tr2("Не вдалося зберегти файл:\n%1\n\n%2", "Could not save the file:\n%1\n\n%2").arg(path, err));
        return QString();
    }
    Settings::setValue(QStringLiteral("general/lastSaveAsDir"), QFileInfo(path).absolutePath());
    return path;
}

bool printImage(QWidget *parent, const QImage &img)
{
#if QT_CONFIG(printdialog)
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dlg(&printer, parent);
    dlg.setWindowTitle(tr2("Друк знімка", "Print screenshot"));
    if (dlg.exec() != QDialog::Accepted)
        return false;

    QPainter p(&printer);
    const QRect page = p.viewport();
    QSize size = img.size();
    if (size.width() > page.width() || size.height() > page.height())
        size.scale(page.size(), Qt::KeepAspectRatio);
    p.setViewport(page.x(), page.y(), size.width(), size.height());
    p.setWindow(img.rect());
    p.drawImage(0, 0, img);
    return true;
#else
    Q_UNUSED(parent)
    Q_UNUSED(img)
    return false;
#endif
}

bool removeFile(const QString &path, bool toTrash, QString *error)
{
    if (toTrash) {
        if (QFile::moveToTrash(path))
            return true;
        if (error)
            *error = tr2("Не вдалося перемістити в кошик", "Could not move to the Recycle Bin");
        return false;
    }
    QFile f(path);
    if (f.remove())
        return true;
    if (error)
        *error = f.errorString();
    return false;
}

void showInFolder(const QString &path)
{
#ifdef Q_OS_WIN
    QProcess::startDetached(QStringLiteral("explorer.exe"),
                            {QStringLiteral("/select,"), QDir::toNativeSeparators(path)});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

void openFolder(const QString &dir)
{
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

QString humanSize(qint64 bytes)
{
    if (bytes < 1024)
        return tr2("%1 Б", "%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return tr2("%1 КБ", "%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024)
        return tr2("%1 МБ", "%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return tr2("%1 ГБ", "%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

} // namespace ImageIO
