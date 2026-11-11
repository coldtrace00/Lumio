#pragma once

#include <QImage>
#include <QString>
#include <QStringList>

class QWidget;

// Збереження, копіювання, друк та видалення зображень.
namespace ImageIO {

QStringList nameFilters();                 // *.png *.jpg ...
QString nextGalleryPath();                 // унікальне ім'я файлу в папці галереї
QString saveToGallery(const QImage &img, QString *error = nullptr); // повертає шлях або ""
bool saveImage(const QImage &img, const QString &path, QString *error = nullptr);
void copyToClipboard(const QImage &img);
void copyFilesToClipboard(const QStringList &paths, const QImage &previewImage = QImage());
QString saveAsDialog(QWidget *parent, const QImage &img);
bool printImage(QWidget *parent, const QImage &img);
bool removeFile(const QString &path, bool toTrash, QString *error = nullptr);
void showInFolder(const QString &path);
void openFolder(const QString &dir);
QString humanSize(qint64 bytes);

} // namespace ImageIO
