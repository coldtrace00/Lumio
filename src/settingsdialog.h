#pragma once

#include <QDialog>

class HotkeyManager;
class QCheckBox;
class QComboBox;
class QPushButton;
class QLabel;
class QLineEdit;
class QSpinBox;

// Редактор глобальної гарячої клавіші: модифікатори + клавіша зі списку
// або кнопка «Записати», яка запам'ятовує будь-яку натиснуту клавішу
// (зокрема спецкнопки Logitech, PrtSc, Win+Shift+S).
class HotkeyEdit : public QWidget
{
    Q_OBJECT
public:
    explicit HotkeyEdit(QWidget *parent = nullptr);
    void setSequence(const QString &combo);
    QString sequence() const;
    QPushButton *recordButton() const { return m_record; }

signals:
    void recordRequested(HotkeyEdit *self);

private:
    QPushButton *m_record;
    QCheckBox *m_ctrl;
    QCheckBox *m_alt;
    QCheckBox *m_shift;
    QCheckBox *m_win;
    QComboBox *m_key;
};

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(HotkeyManager *hotkeys, QWidget *parent = nullptr);

    void accept() override;
    void reject() override;
    bool languageChanged() const { return m_languageChanged; }

private:
    void startRecording(HotkeyEdit *edit);
    void finishRecording(const QString &combo);
    HotkeyManager *m_hotkeys;
    HotkeyEdit *m_recordingEdit = nullptr;
    QList<HotkeyEdit *> m_hotkeyEdits;
    void load();
    void updatePreview();

    QComboBox *m_language;
    bool m_languageChanged = false;
    QLineEdit *m_saveDir;
    QComboBox *m_format;
    QSpinBox *m_quality;
    QLineEdit *m_pattern;
    QLabel *m_patternPreview;

    QCheckBox *m_autoSaveOnCopy;
    QCheckBox *m_copyOnSave;
    QCheckBox *m_notifications;
    QCheckBox *m_magnifier;
    QCheckBox *m_detectWindows;
    QCheckBox *m_autostart;
    QCheckBox *m_confirmDelete;
    QCheckBox *m_recycleBin;

    HotkeyEdit *m_hkRegion;
    HotkeyEdit *m_hkRegion2;
    HotkeyEdit *m_hkFullscreen;
    HotkeyEdit *m_hkGallery;
};
