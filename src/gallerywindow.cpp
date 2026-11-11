#include "gallerywindow.h"
#include "i18n.h"

#include "hotkeymanager.h"
#include "icons.h"
#include "imageio.h"
#include "settings.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QImageReader>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPointer>
#include <QRunnable>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QThread>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

#include <utility>

namespace {
const int kThumbMax = 256;
const int kPathRole = Qt::UserRole;
}

// ---------------------------------------------------------------------------
// ThumbList

QStringList ThumbList::mimeTypes() const
{
    return {QStringLiteral("text/uri-list")};
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
QMimeData *ThumbList::mimeData(const QList<QListWidgetItem *> &items) const
#else
QMimeData *ThumbList::mimeData(const QList<QListWidgetItem *> items) const
#endif
{
    auto *md = new QMimeData;
    QList<QUrl> urls;
    for (QListWidgetItem *it : items)
        urls << QUrl::fromLocalFile(it->data(kPathRole).toString());
    md->setUrls(urls);
    return md;
}

// ---------------------------------------------------------------------------
// GalleryWindow

GalleryWindow::GalleryWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr2("Lumio — Галерея", "Lumio — Gallery"));
    setWindowIcon(Icons::appIcon());
    m_pool.setMaxThreadCount(qMax(1, QThread::idealThreadCount() - 1));

    m_placeholder = QPixmap(kThumbMax, kThumbMax);
    m_placeholder.fill(Qt::transparent);
    {
        QPainter p(&m_placeholder);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(128, 128, 128, 50));
        p.drawRoundedRect(QRectF(16, 40, kThumbMax - 32, kThumbMax - 80), 12, 12);
    }

    buildUi();

    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this]() { m_reloadTimer.start(); });
    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(300);
    connect(&m_reloadTimer, &QTimer::timeout, this, &GalleryWindow::reload);

    const QByteArray geo = Settings::value(QStringLiteral("gallery/geometry")).toByteArray();
    if (!geo.isEmpty())
        restoreGeometry(geo);
    else
        resize(1100, 700);
    const QByteArray split = Settings::value(QStringLiteral("gallery/splitter")).toByteArray();
    if (!split.isEmpty())
        m_splitter->restoreState(split);

    reload();
}

GalleryWindow::~GalleryWindow()
{
    m_pool.clear();
    m_pool.waitForDone();
}

void GalleryWindow::buildUi()
{
    // --- панель інструментів
    QToolBar *tb = addToolBar(tr2("Головна", "Main"));
    tb->setObjectName(QStringLiteral("mainToolbar"));
    tb->setMovable(false);
    tb->setIconSize(QSize(20, 20));
    tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    QAction *aCapture = tb->addAction(Icons::themed(Icons::Capture), tr2("Новий знімок", "New screenshot"));
    aCapture->setShortcut(QKeySequence::New);
    connect(aCapture, &QAction::triggered, this, &GalleryWindow::captureRequested);
    tb->addSeparator();

    m_aOpen = new QAction(Icons::themed(Icons::Open), tr2("Відкрити", "Open"), this);
    connect(m_aOpen, &QAction::triggered, this, &GalleryWindow::openSelected);

    m_aCopy = new QAction(Icons::themed(Icons::Copy), tr2("Копіювати", "Copy"), this);
    m_aCopy->setShortcut(QKeySequence::Copy);
    connect(m_aCopy, &QAction::triggered, this, &GalleryWindow::copySelected);

    m_aSaveAs = new QAction(Icons::themed(Icons::SaveAs), tr2("Зберегти як…", "Save as…"), this);
    m_aSaveAs->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")));
    connect(m_aSaveAs, &QAction::triggered, this, &GalleryWindow::saveAsSelected);

    m_aPin = new QAction(Icons::themed(Icons::Pin), tr2("Закріпити", "Pin"), this);
    m_aPin->setShortcut(QKeySequence(QStringLiteral("Ctrl+T")));
    m_aPin->setToolTip(tr2("Закріпити знімок поверх усіх вікон (Ctrl+T)", "Pin the screenshot on top of all windows (Ctrl+T)"));
    connect(m_aPin, &QAction::triggered, this, &GalleryWindow::pinSelected);

    m_aRename = new QAction(Icons::themed(Icons::Rename), tr2("Перейменувати", "Rename"), this);
    m_aRename->setShortcut(QKeySequence(Qt::Key_F2));
    connect(m_aRename, &QAction::triggered, this, &GalleryWindow::renameCurrent);

    m_aDelete = new QAction(Icons::themed(Icons::Trash), tr2("Видалити", "Delete"), this);
    m_aDelete->setShortcut(QKeySequence::Delete);
    connect(m_aDelete, &QAction::triggered, this, [this]() { deleteSelected(false); });

    m_aDeletePermanent = new QAction(tr2("Видалити назавжди", "Delete permanently"), this);
    m_aDeletePermanent->setShortcut(QKeySequence(QStringLiteral("Shift+Del")));
    connect(m_aDeletePermanent, &QAction::triggered, this, [this]() { deleteSelected(true); });

    m_aShowInFolder = new QAction(Icons::themed(Icons::Folder), tr2("Показати в папці", "Show in folder"), this);
    connect(m_aShowInFolder, &QAction::triggered, this, &GalleryWindow::showInFolder);

    for (QAction *a : {m_aOpen, m_aCopy, m_aSaveAs, m_aPin, m_aRename, m_aDelete, m_aDeletePermanent, m_aShowInFolder})
        addAction(a); // щоб гарячі клавіші працювали у всьому вікні

    tb->addAction(m_aCopy);
    tb->addAction(m_aSaveAs);
    tb->addAction(m_aPin);
    tb->addAction(m_aDelete);
    tb->addSeparator();

    QAction *aFolder = tb->addAction(Icons::themed(Icons::Folder), tr2("Папка", "Folder"));
    aFolder->setToolTip(tr2("Відкрити папку зі знімками", "Open the screenshots folder"));
    connect(aFolder, &QAction::triggered, this, []() { ImageIO::openFolder(Settings::saveDir()); });

    QAction *aRefresh = tb->addAction(Icons::themed(Icons::Refresh), tr2("Оновити", "Refresh"));
    aRefresh->setShortcut(QKeySequence::Refresh);
    connect(aRefresh, &QAction::triggered, this, &GalleryWindow::reload);

    QAction *aSettings = tb->addAction(Icons::themed(Icons::Gear), tr2("Налаштування", "Settings"));
    connect(aSettings, &QAction::triggered, this, &GalleryWindow::settingsRequested);

    // --- рядок пошуку та розміру мініатюр над списком
    auto *left = new QWidget(this);
    auto *ll = new QVBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(4);
    auto *filterBar = new QHBoxLayout;
    filterBar->setContentsMargins(6, 6, 6, 0);

    m_search = new QLineEdit(left);
    m_search->setPlaceholderText(tr2("Пошук за назвою…", "Search by name…"));
    m_search->setClearButtonEnabled(true);
    m_search->setMaximumWidth(280);
    connect(m_search, &QLineEdit::textChanged, this, &GalleryWindow::applyFilter);

    m_sizeSlider = new QSlider(Qt::Horizontal, left);
    m_sizeSlider->setRange(64, kThumbMax);
    m_sizeSlider->setFixedWidth(130);
    m_sizeSlider->setToolTip(tr2("Розмір мініатюр", "Thumbnail size"));

    filterBar->addWidget(m_search, 1);
    filterBar->addStretch();
    filterBar->addWidget(new QLabel(tr2("Розмір:", "Size:"), left));
    filterBar->addWidget(m_sizeSlider);
    ll->addLayout(filterBar);

    // --- список мініатюр
    m_list = new ThumbList(left);
    ll->addWidget(m_list, 1);
    m_list->setViewMode(QListView::IconMode);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setUniformItemSizes(true);
    m_list->setWordWrap(false);
    m_list->setTextElideMode(Qt::ElideMiddle);
    m_list->setSpacing(6);
    m_list->setDragEnabled(true);
    m_list->setDragDropMode(QAbstractItemView::DragOnly);
    m_list->setDefaultDropAction(Qt::CopyAction);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QListWidget::customContextMenuRequested, this, &GalleryWindow::showContextMenu);
    connect(m_list, &QListWidget::itemActivated, this, [this]() { openSelected(); });
    connect(m_list, &QListWidget::itemSelectionChanged, this, [this]() {
        updatePreview();
        updateActions();
    });

    connect(m_sizeSlider, &QSlider::valueChanged, this, [this](int v) {
        m_list->setIconSize(QSize(v, v));
        m_list->setGridSize(QSize(v + 24, v + 34));
        Settings::setValue(QStringLiteral("gallery/thumbSize"), v);
    });
    m_sizeSlider->setValue(qBound(64, Settings::value(QStringLiteral("gallery/thumbSize"), 160).toInt(), kThumbMax));
    m_list->setIconSize(QSize(m_sizeSlider->value(), m_sizeSlider->value()));
    m_list->setGridSize(QSize(m_sizeSlider->value() + 24, m_sizeSlider->value() + 34));

    // --- панель попереднього перегляду
    auto *side = new QWidget(this);
    auto *sl = new QVBoxLayout(side);
    sl->setContentsMargins(8, 8, 8, 8);
    m_preview = new QLabel(side);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumSize(220, 160);
    m_preview->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_preview->setStyleSheet(QStringLiteral("QLabel { background: rgba(127,127,127,25); border-radius: 8px; }"));
    m_preview->installEventFilter(this);
    m_info = new QLabel(side);
    m_info->setWordWrap(true);
    m_info->setTextInteractionFlags(Qt::TextSelectableByMouse);
    sl->addWidget(m_preview, 1);
    sl->addWidget(m_info);

    m_splitter = new QSplitter(this);
    m_splitter->addWidget(left);
    m_splitter->addWidget(side);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 2);
    setCentralWidget(m_splitter);

    m_status = new QLabel(this);
    statusBar()->addWidget(m_status, 1);

    updateActions();
}

void GalleryWindow::closeEvent(QCloseEvent *e)
{
    Settings::setValue(QStringLiteral("gallery/geometry"), saveGeometry());
    Settings::setValue(QStringLiteral("gallery/splitter"), m_splitter->saveState());
    e->accept();
}

bool GalleryWindow::eventFilter(QObject *obj, QEvent *e)
{
    if (obj == m_preview && e->type() == QEvent::Resize)
        rescalePreview();
    return QMainWindow::eventFilter(obj, e);
}

// ---------------------------------------------------------------------------

void GalleryWindow::reload()
{
    const QString dirPath = Settings::saveDir();
    QDir dir(dirPath);
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));

    if (!m_watcher->directories().isEmpty())
        m_watcher->removePaths(m_watcher->directories());
    m_watcher->addPath(dir.absolutePath());

    const QStringList previouslySelected = selectedPaths();
    const QString previousCurrent = currentPath();

    const QFileInfoList files = dir.entryInfoList(ImageIO::nameFilters(), QDir::Files, QDir::Time);

    m_list->blockSignals(true);
    m_list->clear();
    m_items.clear();
    QStringList paths;
    for (const QFileInfo &fi : files) {
        const QString path = fi.absoluteFilePath();
        auto *item = new QListWidgetItem(QIcon(m_placeholder), fi.completeBaseName());
        item->setData(kPathRole, path);
        item->setToolTip(QStringLiteral("%1\n%2  •  %3")
                             .arg(fi.fileName(),
                                  fi.lastModified().toString(QStringLiteral("dd.MM.yyyy HH:mm:ss")),
                                  ImageIO::humanSize(fi.size())));
        m_list->addItem(item);
        m_items.insert(path, item);
        paths << path;
        if (previouslySelected.contains(path))
            item->setSelected(true);
        if (path == previousCurrent)
            m_list->setCurrentItem(item, QItemSelectionModel::NoUpdate);
    }
    m_list->blockSignals(false);

    applyFilter();
    loadThumbnails(paths);
    updateStatus();
    updatePreview();
    updateActions();
}

void GalleryWindow::selectPath(const QString &path)
{
    QListWidgetItem *it = m_items.value(QFileInfo(path).absoluteFilePath());
    if (!it)
        return;
    m_list->clearSelection();
    m_list->setCurrentItem(it);
    it->setSelected(true);
    m_list->scrollToItem(it);
}

QStringList GalleryWindow::selectedPaths() const
{
    QStringList out;
    if (!m_list)
        return out;
    // зберігаємо порядок відображення
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *it = m_list->item(i);
        if (it->isSelected() && !it->isHidden())
            out << it->data(kPathRole).toString();
    }
    return out;
}

QString GalleryWindow::currentPath() const
{
    const QStringList sel = selectedPaths();
    if (sel.size() == 1)
        return sel.first();
    QListWidgetItem *it = m_list ? m_list->currentItem() : nullptr;
    return (it && it->isSelected()) ? it->data(kPathRole).toString() : QString();
}

void GalleryWindow::loadThumbnails(const QStringList &paths)
{
    QPointer<GalleryWindow> self(this);
    for (const QString &path : paths) {
        const QString key = path + QLatin1Char('|')
                            + QString::number(QFileInfo(path).lastModified().toMSecsSinceEpoch());
        const auto cached = m_cache.constFind(key);
        if (cached != m_cache.constEnd()) {
            if (QListWidgetItem *it = m_items.value(path))
                it->setIcon(QIcon(QPixmap::fromImage(cached.value())));
            continue;
        }
        m_pool.start(QRunnable::create([self, path, key]() {
            QImageReader reader(path);
            reader.setAutoTransform(true);
            QSize s = reader.size();
            if (s.isValid() && (s.width() > kThumbMax || s.height() > kThumbMax)) {
                s.scale(kThumbMax, kThumbMax, Qt::KeepAspectRatio);
                reader.setScaledSize(s);
            }
            QImage img = reader.read();
            if (!img.isNull() && (img.width() > kThumbMax || img.height() > kThumbMax))
                img = img.scaled(kThumbMax, kThumbMax, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QMetaObject::invokeMethod(
                qApp,
                [self, path, key, img]() {
                    if (self)
                        self->onThumbReady(path, key, img);
                },
                Qt::QueuedConnection);
        }));
    }
}

void GalleryWindow::onThumbReady(const QString &path, const QString &key, const QImage &img)
{
    if (img.isNull())
        return;
    if (m_cache.size() > 3000)
        m_cache.clear();
    // Квадратна мініатюра з картинкою по центру — підписи стоять рівно під кожною
    QImage square(kThumbMax, kThumbMax, QImage::Format_ARGB32_Premultiplied);
    square.fill(Qt::transparent);
    {
        QPainter p(&square);
        p.drawImage((kThumbMax - img.width()) / 2, (kThumbMax - img.height()) / 2, img);
    }
    m_cache.insert(key, square);
    if (QListWidgetItem *it = m_items.value(path))
        it->setIcon(QIcon(QPixmap::fromImage(square)));
}

void GalleryWindow::applyFilter()
{
    const QString text = m_search->text().trimmed();
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *it = m_list->item(i);
        it->setHidden(!text.isEmpty() && !it->text().contains(text, Qt::CaseInsensitive));
    }
    updateStatus();
}

void GalleryWindow::updateStatus()
{
    qint64 total = 0;
    int visible = 0;
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *it = m_list->item(i);
        if (it->isHidden())
            continue;
        ++visible;
        total += QFileInfo(it->data(kPathRole).toString()).size();
    }
    m_status->setText(tr2("Знімків: %1  •  %2  •  %3", "Screenshots: %1  •  %2  •  %3")
                          .arg(visible)
                          .arg(ImageIO::humanSize(total), QDir::toNativeSeparators(Settings::saveDir())));
}

void GalleryWindow::updateActions()
{
    const int n = selectedPaths().size();
    m_aOpen->setEnabled(n > 0);
    m_aCopy->setEnabled(n > 0);
    m_aSaveAs->setEnabled(n == 1);
    m_aPin->setEnabled(n == 1);
    m_aRename->setEnabled(n == 1);
    m_aDelete->setEnabled(n > 0);
    m_aDeletePermanent->setEnabled(n > 0);
    m_aShowInFolder->setEnabled(n > 0);
}

void GalleryWindow::updatePreview()
{
    const QStringList sel = selectedPaths();
    if (sel.size() != 1) {
        m_previewPath.clear();
        m_previewPixmap = QPixmap();
        m_preview->setPixmap(QPixmap());
        if (sel.isEmpty()) {
            m_preview->setText(m_list->count() == 0
                                   ? tr2("Знімків ще немає.\n\nНатисніть %1, щоб зробити перший.", "No screenshots yet.\n\nPress %1 to take the first one.")
                                         .arg(HotkeyManager::displayText(Settings::hotkey(QStringLiteral("region"))))
                                   : tr2("Виберіть знімок", "Select a screenshot"));
            m_info->clear();
        } else {
            m_preview->setText(tr2("Вибрано знімків: %1", "Selected screenshots: %1").arg(sel.size()));
            qint64 total = 0;
            for (const QString &p : sel)
                total += QFileInfo(p).size();
            m_info->setText(tr2("Загальний розмір: %1", "Total size: %1").arg(ImageIO::humanSize(total)));
        }
        return;
    }

    const QString path = sel.first();
    if (path == m_previewPath && !m_previewPixmap.isNull())
        return;
    m_previewPath = path;
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage img = reader.read();
    m_previewPixmap = QPixmap::fromImage(img);
    m_preview->setText(img.isNull() ? tr2("Не вдалося відкрити зображення", "Could not open the image") : QString());
    rescalePreview();

    const QFileInfo fi(path);
    m_info->setText(tr2("<b>%1</b><br>%2 × %3 пікс.  •  %4<br>%5", "<b>%1</b><br>%2 × %3 px  •  %4<br>%5")
                        .arg(fi.fileName().toHtmlEscaped())
                        .arg(img.width())
                        .arg(img.height())
                        .arg(ImageIO::humanSize(fi.size()),
                             fi.lastModified().toString(QStringLiteral("dd.MM.yyyy HH:mm:ss"))));
}

void GalleryWindow::rescalePreview()
{
    if (m_previewPixmap.isNull())
        return;
    const qreal dpr = m_preview->devicePixelRatioF();
    const QSize target = m_preview->size() * dpr - QSize(16, 16);
    QPixmap pm = m_previewPixmap;
    if (pm.width() > target.width() || pm.height() > target.height())
        pm = pm.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    pm.setDevicePixelRatio(dpr);
    m_preview->setPixmap(pm);
}

void GalleryWindow::showContextMenu(const QPoint &pos)
{
    QListWidgetItem *it = m_list->itemAt(pos);
    if (it && !it->isSelected()) {
        m_list->clearSelection();
        it->setSelected(true);
        m_list->setCurrentItem(it);
    }
    QMenu menu(this);
    if (!selectedPaths().isEmpty()) {
        menu.addAction(m_aOpen);
        menu.addAction(m_aCopy);
        menu.addAction(m_aSaveAs);
        menu.addAction(m_aPin);
        menu.addSeparator();
        menu.addAction(m_aRename);
        menu.addAction(m_aShowInFolder);
        menu.addSeparator();
        menu.addAction(m_aDelete);
        menu.addAction(m_aDeletePermanent);
    } else {
        menu.addAction(Icons::themed(Icons::Capture), tr2("Новий знімок", "New screenshot"), this,
                       &GalleryWindow::captureRequested);
        menu.addAction(Icons::themed(Icons::Folder), tr2("Відкрити папку", "Open folder"), this,
                       []() { ImageIO::openFolder(Settings::saveDir()); });
    }
    menu.exec(m_list->viewport()->mapToGlobal(pos));
}

// ---------------------------------------------------------------------------
// Дії

void GalleryWindow::openSelected()
{
    const QStringList paths = selectedPaths();
    int n = 0;
    for (const QString &p : paths) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(p));
        if (++n >= 10)
            break;
    }
}

void GalleryWindow::copySelected()
{
    const QStringList paths = selectedPaths();
    if (paths.isEmpty())
        return;
    // Одночасно і файл (вставка в провідник/месенджер), і зображення (вставка в Paint/Word)
    QImage img;
    if (paths.size() == 1)
        img = QImage(paths.first());
    ImageIO::copyFilesToClipboard(paths, img);
    statusBar()->showMessage(paths.size() == 1 ? tr2("Скопійовано в буфер обміну", "Copied to the clipboard")
                                               : tr2("Скопійовано файлів: %1", "Files copied: %1").arg(paths.size()),
                             2500);
}

void GalleryWindow::saveAsSelected()
{
    const QString path = currentPath();
    if (path.isEmpty())
        return;
    const QImage img(path);
    if (img.isNull())
        return;
    const QString saved = ImageIO::saveAsDialog(this, img);
    if (!saved.isEmpty())
        statusBar()->showMessage(tr2("Збережено: %1", "Saved: %1").arg(QDir::toNativeSeparators(saved)), 4000);
}

void GalleryWindow::pinSelected()
{
    const QString path = currentPath();
    if (path.isEmpty())
        return;
    const QImage img(path);
    if (!img.isNull())
        emit pinRequested(img);
}

void GalleryWindow::renameCurrent()
{
    const QString path = currentPath();
    if (path.isEmpty())
        return;
    const QFileInfo fi(path);
    bool ok = false;
    QString name = QInputDialog::getText(this, tr2("Перейменувати", "Rename"), tr2("Нова назва:", "New name:"),
                                         QLineEdit::Normal, fi.completeBaseName(), &ok)
                       .trimmed();
    if (!ok || name.isEmpty() || name == fi.completeBaseName())
        return;
    static const QString bad = QStringLiteral("<>:\"/\\|?*");
    for (const QChar c : bad) {
        if (name.contains(c)) {
            QMessageBox::warning(this, tr2("Перейменувати", "Rename"),
                                 tr2("Назва не може містити символи: %1", "The name cannot contain these characters: %1").arg(bad));
            return;
        }
    }
    const QString newPath = fi.absoluteDir().filePath(name + QLatin1Char('.') + fi.suffix());
    if (QFile::exists(newPath)) {
        QMessageBox::warning(this, tr2("Перейменувати", "Rename"), tr2("Файл з такою назвою вже існує.", "A file with this name already exists."));
        return;
    }
    if (!QFile::rename(path, newPath)) {
        QMessageBox::warning(this, tr2("Перейменувати", "Rename"), tr2("Не вдалося перейменувати файл.", "Could not rename the file."));
        return;
    }
    reload();
    selectPath(newPath);
}

void GalleryWindow::deleteSelected(bool permanent)
{
    const QStringList paths = selectedPaths();
    if (paths.isEmpty())
        return;

    const bool toTrash = Settings::useRecycleBin() && !permanent;
    if (Settings::confirmDelete() || !toTrash) {
        QString q = paths.size() == 1
                        ? tr2("Видалити «%1»?", "Delete “%1”?").arg(QFileInfo(paths.first()).fileName())
                        : tr2("Видалити вибрані знімки (%1 шт.)?", "Delete the selected screenshots (%1)?").arg(paths.size());
        q += toTrash ? tr2("\n\nФайли буде переміщено в кошик.", "\n\nThe files will be moved to the Recycle Bin.")
                     : tr2("\n\nФайли буде видалено назавжди — цю дію неможливо скасувати.", "\n\nThe files will be deleted permanently — this cannot be undone.");
        if (QMessageBox::question(this, tr2("Видалення", "Delete"), q, QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No)
            != QMessageBox::Yes)
            return;
    }

    // Запам'ятати позицію, щоб після видалення виділити сусідній знімок
    int firstRow = m_list->count();
    for (const QString &p : paths)
        if (QListWidgetItem *it = m_items.value(p))
            firstRow = qMin(firstRow, m_list->row(it));

    QStringList failed;
    for (const QString &p : paths) {
        if (!ImageIO::removeFile(p, toTrash))
            failed << p;
    }

    if (!failed.isEmpty() && toTrash) {
        const auto answer = QMessageBox::question(
            this, tr2("Видалення", "Delete"),
            tr2("Не вдалося перемістити в кошик файлів: %1.\nВидалити їх назавжди?", "Could not move %1 file(s) to the Recycle Bin.\nDelete them permanently?").arg(failed.size()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            QStringList stillFailed;
            for (const QString &p : std::as_const(failed))
                if (!ImageIO::removeFile(p, false))
                    stillFailed << p;
            failed = stillFailed;
        } else {
            failed.clear();
        }
    }
    if (!failed.isEmpty()) {
        QMessageBox::warning(this, tr2("Видалення", "Delete"),
                             tr2("Не вдалося видалити:\n%1", "Could not delete:\n%1").arg(failed.join(QLatin1Char('\n'))));
    }

    m_previewPath.clear();
    reload();
    if (m_list->count() > 0) {
        const int row = qBound(0, firstRow, m_list->count() - 1);
        m_list->clearSelection();
        m_list->setCurrentRow(row);
        if (QListWidgetItem *it = m_list->item(row))
            it->setSelected(true);
    }
    statusBar()->showMessage(tr2("Видалено: %1", "Deleted: %1").arg(paths.size() - failed.size()), 2500);
}

void GalleryWindow::showInFolder()
{
    const QStringList paths = selectedPaths();
    if (!paths.isEmpty())
        ImageIO::showInFolder(paths.first());
}
