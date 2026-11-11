#include "settingsdialog.h"
#include "i18n.h"

#include "hotkeymanager.h"
#include "icons.h"
#include "settings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <utility>

// ---------------------------------------------------------------------------
// HotkeyEdit

HotkeyEdit::HotkeyEdit(QWidget *parent)
    : QWidget(parent)
{
    auto *l = new QHBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    m_ctrl = new QCheckBox(QStringLiteral("Ctrl"), this);
    m_alt = new QCheckBox(QStringLiteral("Alt"), this);
    m_shift = new QCheckBox(QStringLiteral("Shift"), this);
    m_win = new QCheckBox(QStringLiteral("Win"), this);
    m_key = new QComboBox(this);
    m_key->setMinimumWidth(150);
    m_record = new QPushButton(tr2("Записати…", "Record…"), this);
    m_record->setToolTip(tr2("Натисніть, а потім натисніть потрібну клавішу або комбінацію", "Click, then press the key or key combination you want"));
    connect(m_record, &QPushButton::clicked, this, [this]() { emit recordRequested(this); });

    m_key->addItem(tr2("— вимкнено —", "— disabled —"), QString());
    auto add = [this](Qt::Key k, const QString &label = QString()) {
        const QString portable = QKeySequence(int(k)).toString(QKeySequence::PortableText);
        m_key->addItem(label.isEmpty() ? portable : label, portable);
    };
    add(Qt::Key_Print, QStringLiteral("PrtSc (Print Screen)"));
    add(Qt::Key_Pause);
    add(Qt::Key_ScrollLock, QStringLiteral("Scroll Lock"));
    add(Qt::Key_Insert);
    add(Qt::Key_Home);
    add(Qt::Key_End);
    add(Qt::Key_PageUp);
    add(Qt::Key_PageDown);
    for (int k = Qt::Key_F1; k <= Qt::Key_F24; ++k)
        add(Qt::Key(k));
    for (int k = Qt::Key_A; k <= Qt::Key_Z; ++k)
        add(Qt::Key(k));
    for (int k = Qt::Key_0; k <= Qt::Key_9; ++k)
        add(Qt::Key(k));

    l->addWidget(m_ctrl);
    l->addWidget(m_alt);
    l->addWidget(m_shift);
    l->addWidget(m_win);
    l->addWidget(new QLabel(QStringLiteral("+"), this));
    l->addWidget(m_key, 1);
    l->addWidget(m_record);
}

void HotkeyEdit::setSequence(const QString &combo)
{
    m_ctrl->setChecked(false);
    m_alt->setChecked(false);
    m_shift->setChecked(false);
    m_win->setChecked(false);
    m_key->setCurrentIndex(0);

    unsigned vk = 0, mods = 0;
    if (!HotkeyManager::parse(combo, vk, mods))
        return;
    m_ctrl->setChecked(mods & HotkeyManager::ModCtrl);
    m_alt->setChecked(mods & HotkeyManager::ModAlt);
    m_shift->setChecked(mods & HotkeyManager::ModShift);
    m_win->setChecked(mods & HotkeyManager::ModWin);
    const QString name = HotkeyManager::keyName(vk);
    int idx = m_key->findData(name);
    if (idx < 0) { // нестандартна клавіша, наприклад спецкнопка клавіатури
        m_key->addItem(HotkeyManager::displayText(name), name);
        idx = m_key->count() - 1;
    }
    m_key->setCurrentIndex(idx);
}

QString HotkeyEdit::sequence() const
{
    const QString key = m_key->currentData().toString();
    if (key.isEmpty())
        return QString();
    QStringList parts;
    if (m_ctrl->isChecked())  parts << QStringLiteral("Ctrl");
    if (m_alt->isChecked())   parts << QStringLiteral("Alt");
    if (m_shift->isChecked()) parts << QStringLiteral("Shift");
    if (m_win->isChecked())   parts << QStringLiteral("Meta");
    parts << key;
    return parts.join(QLatin1Char('+'));
}

// ---------------------------------------------------------------------------
// SettingsDialog

SettingsDialog::SettingsDialog(HotkeyManager *hotkeys, QWidget *parent)
    : QDialog(parent)
    , m_hotkeys(hotkeys)
{
    setWindowTitle(tr2("Lumio — Налаштування", "Lumio — Settings"));
    setWindowIcon(Icons::appIcon());
    setMinimumWidth(560);

    auto *tabs = new QTabWidget(this);

    // --- Загальні
    auto *general = new QWidget(tabs);
    auto *gl = new QVBoxLayout(general);

    // Мова інтерфейсу (назва навмисно двома мовами — щоб її знайшли будь-якою)
    auto *langRow = new QHBoxLayout;
    langRow->addWidget(new QLabel(QStringLiteral("Мова / Language:"), general));
    m_language = new QComboBox(general);
    m_language->addItem(QStringLiteral("Українська"), QStringLiteral("uk"));
    m_language->addItem(QStringLiteral("English"), QStringLiteral("en"));
    langRow->addWidget(m_language);
    langRow->addStretch();
    gl->addLayout(langRow);

    auto *saveBox = new QGroupBox(tr2("Збереження", "Saving"), general);
    auto *form = new QFormLayout(saveBox);

    m_saveDir = new QLineEdit(saveBox);
    auto *browse = new QPushButton(tr2("Огляд…", "Browse…"), saveBox);
    auto *dirRow = new QHBoxLayout;
    dirRow->addWidget(m_saveDir, 1);
    dirRow->addWidget(browse);
    form->addRow(tr2("Папка галереї:", "Gallery folder:"), dirRow);
    connect(browse, &QPushButton::clicked, this, [this]() {
        const QString d = QFileDialog::getExistingDirectory(this, tr2("Папка для знімків", "Folder for screenshots"), m_saveDir->text());
        if (!d.isEmpty())
            m_saveDir->setText(QDir::toNativeSeparators(d));
    });

    m_format = new QComboBox(saveBox);
    m_format->addItem(tr2("PNG (без втрат, рекомендовано)", "PNG (lossless, recommended)"), QStringLiteral("png"));
    m_format->addItem(tr2("JPG (менший розмір)", "JPG (smaller files)"), QStringLiteral("jpg"));
    m_quality = new QSpinBox(saveBox);
    m_quality->setRange(1, 100);
    m_quality->setSuffix(QStringLiteral(" %"));
    auto *fmtRow = new QHBoxLayout;
    fmtRow->addWidget(m_format, 1);
    fmtRow->addWidget(new QLabel(tr2("Якість JPG:", "JPG quality:"), saveBox));
    fmtRow->addWidget(m_quality);
    form->addRow(tr2("Формат:", "Format:"), fmtRow);
    connect(m_format, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this]() { m_quality->setEnabled(m_format->currentData().toString() == QLatin1String("jpg")); });

    m_pattern = new QLineEdit(saveBox);
    m_pattern->setToolTip(tr2("Формат дати Qt: yyyy — рік, MM — місяць, dd — день,\n"
                                         "HH — години, mm — хвилини, ss — секунди, zzz — мілісекунди.\n"
                                         "Звичайний текст беріть в 'одинарні лапки'.", "Qt date format: yyyy — year, MM — month, dd — day,\nHH — hours, mm — minutes, ss — seconds, zzz — milliseconds.\nPut plain text in 'single quotes'."));
    m_patternPreview = new QLabel(saveBox);
    m_patternPreview->setStyleSheet(QStringLiteral("color: gray;"));
    form->addRow(tr2("Шаблон імені:", "File name pattern:"), m_pattern);
    form->addRow(QString(), m_patternPreview);
    connect(m_pattern, &QLineEdit::textChanged, this, &SettingsDialog::updatePreview);
    connect(m_format, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsDialog::updatePreview);
    gl->addWidget(saveBox);

    auto *behBox = new QGroupBox(tr2("Поведінка", "Behavior"), general);
    auto *bl = new QVBoxLayout(behBox);
    m_autoSaveOnCopy = new QCheckBox(tr2("При копіюванні (Ctrl+C) також зберігати знімок у галерею", "When copying (Ctrl+C), also save the screenshot to the gallery"), behBox);
    m_copyOnSave = new QCheckBox(tr2("При збереженні (Ctrl+S) також копіювати в буфер обміну", "When saving (Ctrl+S), also copy to the clipboard"), behBox);
    m_notifications = new QCheckBox(tr2("Показувати сповіщення", "Show notifications"), behBox);
    m_magnifier = new QCheckBox(tr2("Показувати лупу з кольором пікселя під час виділення", "Show a magnifier with the pixel color while selecting"), behBox);
    m_detectWindows = new QCheckBox(tr2("Підсвічувати вікно під курсором (клік — знімок вікна)", "Highlight the window under the cursor (click — capture that window)"), behBox);
    m_autostart = new QCheckBox(tr2("Запускати разом з Windows", "Start with Windows"), behBox);
    for (QCheckBox *c : {m_autoSaveOnCopy, m_copyOnSave, m_notifications, m_magnifier, m_detectWindows, m_autostart})
        bl->addWidget(c);
    gl->addWidget(behBox);
    gl->addStretch();
    tabs->addTab(general, tr2("Загальні", "General"));

    // --- Галерея
    auto *gallery = new QWidget(tabs);
    auto *gal = new QVBoxLayout(gallery);
    m_confirmDelete = new QCheckBox(tr2("Питати підтвердження перед видаленням", "Ask for confirmation before deleting"), gallery);
    m_recycleBin = new QCheckBox(tr2("Видаляти в кошик (а не назавжди)", "Delete to the Recycle Bin (not permanently)"), gallery);
    gal->addWidget(m_confirmDelete);
    gal->addWidget(m_recycleBin);
    auto *galHint = new QLabel(tr2("Порада: Shift+Delete у галереї завжди видаляє назавжди.", "Tip: Shift+Delete in the gallery always deletes permanently."), gallery);
    galHint->setStyleSheet(QStringLiteral("color: gray;"));
    gal->addWidget(galHint);
    gal->addStretch();
    tabs->addTab(gallery, tr2("Галерея", "Gallery"));

    // --- Гарячі клавіші
    auto *keys = new QWidget(tabs);
    auto *kl = new QFormLayout(keys);
    m_hkRegion = new HotkeyEdit(keys);
    m_hkRegion2 = new HotkeyEdit(keys);
    m_hkFullscreen = new HotkeyEdit(keys);
    m_hkGallery = new HotkeyEdit(keys);
    m_hotkeyEdits = {m_hkRegion, m_hkRegion2, m_hkFullscreen, m_hkGallery};
    for (HotkeyEdit *e : std::as_const(m_hotkeyEdits))
        connect(e, &HotkeyEdit::recordRequested, this, &SettingsDialog::startRecording);
    kl->addRow(tr2("Виділити область:", "Select area:"), m_hkRegion);
    kl->addRow(tr2("…або ця клавіша:", "…or this key:"), m_hkRegion2);
    kl->addRow(tr2("Весь екран одразу:", "Full screen at once:"), m_hkFullscreen);
    kl->addRow(tr2("Відкрити галерею:", "Open gallery:"), m_hkGallery);
    auto *keyHint = new QLabel(
        tr2("Програма сама перехоплює клавіші, тож PrtSc і Win+Shift+S більше не відкриватимуть «Ножиці».\n"
                       "Кнопка скріншота на Logitech MX Keys зазвичай надсилає Win+Shift+S або PrtSc — обидві вже "
                       "призначені. Якщо ваша клавіша не спрацьовує, натисніть «Записати…» і натисніть її.", "The program captures the keys itself, so PrtSc and Win+Shift+S no longer open the Snipping Tool.\nThe screenshot key on Logitech MX Keys usually sends Win+Shift+S or PrtSc — both are already assigned. If your key does not work, click “Record…” and press it."),
        keys);
    keyHint->setWordWrap(true);
    keyHint->setStyleSheet(QStringLiteral("color: gray;"));
    kl->addRow(keyHint);
    auto *resetKeys = new QPushButton(tr2("Скинути за замовчуванням", "Reset to defaults"), keys);
    connect(resetKeys, &QPushButton::clicked, this, [this]() {
        m_hkRegion->setSequence(Settings::defaultHotkey(QStringLiteral("region")));
        m_hkRegion2->setSequence(Settings::defaultHotkey(QStringLiteral("region2")));
        m_hkFullscreen->setSequence(Settings::defaultHotkey(QStringLiteral("fullscreen")));
        m_hkGallery->setSequence(Settings::defaultHotkey(QStringLiteral("gallery")));
    });
    kl->addRow(QString(), resetKeys);
    tabs->addTab(keys, tr2("Гарячі клавіші", "Hotkeys"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr2("Зберегти", "Save"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr2("Скасувати", "Cancel"));
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);

    auto *main = new QVBoxLayout(this);
    main->addWidget(tabs);
    main->addWidget(buttons);

    load();
}

void SettingsDialog::load()
{
    m_language->setCurrentIndex(I18n::isUkrainian() ? 0 : 1);
    m_saveDir->setText(QDir::toNativeSeparators(Settings::saveDir()));
    m_format->setCurrentIndex(Settings::imageFormat() == QLatin1String("jpg") ? 1 : 0);
    m_quality->setValue(Settings::jpgQuality());
    m_quality->setEnabled(Settings::imageFormat() == QLatin1String("jpg"));
    m_pattern->setText(Settings::fileNamePattern());

    m_autoSaveOnCopy->setChecked(Settings::autoSaveOnCopy());
    m_copyOnSave->setChecked(Settings::copyOnSave());
    m_notifications->setChecked(Settings::showNotifications());
    m_magnifier->setChecked(Settings::showMagnifier());
    m_detectWindows->setChecked(Settings::detectWindows());
    m_autostart->setChecked(Settings::autostart());
    m_confirmDelete->setChecked(Settings::confirmDelete());
    m_recycleBin->setChecked(Settings::useRecycleBin());

    m_hkRegion->setSequence(Settings::hotkey(QStringLiteral("region")));
    m_hkRegion2->setSequence(Settings::hotkey(QStringLiteral("region2")));
    m_hkFullscreen->setSequence(Settings::hotkey(QStringLiteral("fullscreen")));
    m_hkGallery->setSequence(Settings::hotkey(QStringLiteral("gallery")));
    updatePreview();
}

void SettingsDialog::updatePreview()
{
    QString pattern = m_pattern->text();
    if (pattern.trimmed().isEmpty())
        pattern = Settings::defaultFileNamePattern();
    m_patternPreview->setText(tr2("Приклад: %1.%2", "Example: %1.%2")
                                  .arg(QDateTime::currentDateTime().toString(pattern),
                                       m_format->currentData().toString()));
}

void SettingsDialog::accept()
{
    const bool wantUk = m_language->currentData().toString() == QLatin1String("uk");
    if (wantUk != I18n::isUkrainian()) {
        I18n::setUkrainian(wantUk);
        m_languageChanged = true;
    }
    QString dir = m_saveDir->text().trimmed();
    if (dir.isEmpty())
        dir = Settings::defaultSaveDir();
    Settings::setValue(QStringLiteral("general/saveDir"), QDir::fromNativeSeparators(dir));
    Settings::setValue(QStringLiteral("general/format"), m_format->currentData().toString());
    Settings::setValue(QStringLiteral("general/jpgQuality"), m_quality->value());
    Settings::setValue(QStringLiteral("general/fileNamePattern"), m_pattern->text().trimmed());

    Settings::setValue(QStringLiteral("capture/autoSaveOnCopy"), m_autoSaveOnCopy->isChecked());
    Settings::setValue(QStringLiteral("capture/copyOnSave"), m_copyOnSave->isChecked());
    Settings::setValue(QStringLiteral("capture/showNotifications"), m_notifications->isChecked());
    Settings::setValue(QStringLiteral("capture/showMagnifier"), m_magnifier->isChecked());
    Settings::setValue(QStringLiteral("capture/detectWindows"), m_detectWindows->isChecked());
    Settings::setValue(QStringLiteral("gallery/confirmDelete"), m_confirmDelete->isChecked());
    Settings::setValue(QStringLiteral("gallery/useRecycleBin"), m_recycleBin->isChecked());

    Settings::setValue(QStringLiteral("hotkeys/region"), m_hkRegion->sequence());
    Settings::setValue(QStringLiteral("hotkeys/region2"), m_hkRegion2->sequence());
    Settings::setValue(QStringLiteral("hotkeys/fullscreen"), m_hkFullscreen->sequence());
    Settings::setValue(QStringLiteral("hotkeys/gallery"), m_hkGallery->sequence());

    if (m_autostart->isChecked() != Settings::autostart())
        Settings::setAutostart(m_autostart->isChecked());

    Settings::store().sync();
    if (m_recordingEdit && m_hotkeys)
        m_hotkeys->cancelRecording();
    QDialog::accept();
}

void SettingsDialog::reject()
{
    if (m_recordingEdit && m_hotkeys)
        m_hotkeys->cancelRecording();
    QDialog::reject();
}

void SettingsDialog::startRecording(HotkeyEdit *edit)
{
    if (!m_hotkeys || !m_hotkeys->isActive()) {
        QMessageBox::information(this, tr2("Запис клавіші", "Record key"),
                                 tr2("Запис клавіш доступний лише у Windows.", "Key recording is available only on Windows."));
        return;
    }
    if (m_recordingEdit)
        return;
    m_recordingEdit = edit;
    for (HotkeyEdit *e : std::as_const(m_hotkeyEdits))
        e->recordButton()->setEnabled(false);
    edit->recordButton()->setText(tr2("Натисніть клавішу…", "Press a key…"));
    connect(m_hotkeys, &HotkeyManager::recorded, this, &SettingsDialog::finishRecording, Qt::UniqueConnection);
    m_hotkeys->startRecording();
}

void SettingsDialog::finishRecording(const QString &combo)
{
    disconnect(m_hotkeys, &HotkeyManager::recorded, this, &SettingsDialog::finishRecording);
    if (!m_recordingEdit)
        return;
    if (!combo.isEmpty())
        m_recordingEdit->setSequence(combo);
    m_recordingEdit->recordButton()->setText(tr2("Записати…", "Record…"));
    for (HotkeyEdit *e : std::as_const(m_hotkeyEdits))
        e->recordButton()->setEnabled(true);
    m_recordingEdit = nullptr;
    activateWindow();
}
