#pragma once

#include <QHash>
#include <QImage>
#include <QListWidget>
#include <QMainWindow>
#include <QPixmap>
#include <QThreadPool>
#include <QTimer>

class QAction;
class QFileSystemWatcher;
class QLabel;
class QLineEdit;
class QSlider;
class QSplitter;

// Список мініатюр, який можна перетягувати у провідник / месенджери як файли.
class ThumbList : public QListWidget
{
    Q_OBJECT
public:
    using QListWidget::QListWidget;

protected:
    QStringList mimeTypes() const override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QMimeData *mimeData(const QList<QListWidgetItem *> &items) const override;
#else
    QMimeData *mimeData(const QList<QListWidgetItem *> items) const override;
#endif
};

// Галерея збережених знімків: перегляд, копіювання, перейменування, видалення.
class GalleryWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit GalleryWindow(QWidget *parent = nullptr);
    ~GalleryWindow() override;

    void reload();
    void selectPath(const QString &path);

signals:
    void captureRequested();
    void settingsRequested();
    void pinRequested(const QImage &image);

protected:
    void closeEvent(QCloseEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *e) override;

private:
    void buildUi();
    QStringList selectedPaths() const;
    QString currentPath() const;
    void loadThumbnails(const QStringList &paths);
    void onThumbReady(const QString &path, const QString &key, const QImage &img);
    void applyFilter();
    void updatePreview();
    void rescalePreview();
    void updateStatus();
    void updateActions();
    void showContextMenu(const QPoint &pos);

    void openSelected();
    void copySelected();
    void saveAsSelected();
    void pinSelected();
    void renameCurrent();
    void deleteSelected(bool permanent);
    void showInFolder();

    ThumbList *m_list = nullptr;
    QSplitter *m_splitter = nullptr;
    QLabel *m_preview = nullptr;
    QLabel *m_info = nullptr;
    QLineEdit *m_search = nullptr;
    QSlider *m_sizeSlider = nullptr;
    QLabel *m_status = nullptr;
    QFileSystemWatcher *m_watcher = nullptr;
    QTimer m_reloadTimer;
    QThreadPool m_pool;

    QHash<QString, QListWidgetItem *> m_items; // шлях -> елемент
    QHash<QString, QImage> m_cache;            // шлях+час зміни -> мініатюра
    QPixmap m_placeholder;
    QPixmap m_previewPixmap;
    QString m_previewPath;

    QAction *m_aOpen = nullptr;
    QAction *m_aCopy = nullptr;
    QAction *m_aSaveAs = nullptr;
    QAction *m_aPin = nullptr;
    QAction *m_aRename = nullptr;
    QAction *m_aDelete = nullptr;
    QAction *m_aDeletePermanent = nullptr;
    QAction *m_aShowInFolder = nullptr;
};
