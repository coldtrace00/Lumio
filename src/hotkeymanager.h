#pragma once

#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include <memory>

class QThread;

// Глобальні гарячі клавіші (працюють, навіть коли програма у фоні).
//
// Windows: низькорівневий хук клавіатури (WH_KEYBOARD_LL) в окремому потоці.
// На відміну від RegisterHotKey, він може перехопити й ті комбінації, які
// займає сама Windows: PrtSc («Ножиці») та Win+Shift+S — саме їх надсилають
// кнопки скріншота на клавіатурах Logitech MX Keys та інших.
// Хук лише порівнює натиснуту клавішу з налаштованими комбінаціями і нічого не записує.
//
// Формат комбінації: "Ctrl+Alt+Shift+Meta+<клавіша>", де Meta = Win, а клавіша —
// назва Qt ("Print", "S", "F13") або "VK<число>" для нестандартних клавіш.
class HotkeyManager : public QObject
{
    Q_OBJECT
public:
    enum Modifier : unsigned { ModCtrl = 1, ModAlt = 2, ModShift = 4, ModWin = 8 };

    explicit HotkeyManager(QObject *parent = nullptr);
    ~HotkeyManager() override;

    bool isActive() const; // хук встановлено

    void clear();
    bool addHotkey(int id, const QString &combo); // false — не вдалося розібрати комбінацію

    // Записати наступне натискання (сигнал recorded). Esc — скасувати (recorded("")).
    void startRecording();
    void cancelRecording();

    static bool parse(const QString &combo, unsigned &vk, unsigned &mods);
    static QString compose(unsigned vk, unsigned mods);
    static QString displayText(const QString &combo); // "Win+Shift+S", "PrtSc"
    static QString keyName(unsigned vk);              // "Print", "S", "VK183"

    // Викликається з потоку хука. true — проковтнути натискання.
    bool processKey(unsigned vk, bool down, bool up, unsigned mods);

signals:
    void activated(int id);
    void recorded(const QString &combo);

private:
    struct Combo {
        int id;
        unsigned vk;
        unsigned mods;
    };

    mutable QMutex m_mutex;
    QVector<Combo> m_combos;
    QSet<unsigned> m_swallowUp;
    bool m_recording = false;
    std::unique_ptr<QThread> m_thread;
};
