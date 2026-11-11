#pragma once
// Параметри інсталятора для Lumio (решта коду setup.cpp — спільна з Lumio Photo).

#include <windows.h>

struct AppInfo {
    const wchar_t *name;
    const wchar_t *exe;
    const wchar_t *regKey;            // підрозділ Uninstall і назва в автозапуску
    const wchar_t *dirName;           // папка встановлення за замовчуванням
    const wchar_t *portableDirName;
    const wchar_t *version;
    const wchar_t *const *killNames;  // процеси, які треба закрити перед встановленням (nullptr у кінці)
    bool hasAutostart;
    const wchar_t *autostartArgs;
    const wchar_t *legacyRunName;     // стара назва в автозапуску, яку треба прибрати (або nullptr)
    COLOR16 grad[3][3];               // кольори смужки в шапці (як у лого)
    const wchar_t *taglineUk, *taglineEn;
    const wchar_t *descUk, *descEn;   // опис ярлика
    const wchar_t *portableHintUk, *portableHintEn;
    const char *portableFileText;
    bool settingsInLocalAppData;      // false — %APPDATA%, true — %LOCALAPPDATA%
    const wchar_t *settingsFile;      // відносно (Local)AppData
    const wchar_t *portableSettingsFile; // відносно папки програми
    const wchar_t *dataDirName;       // дані, які можна видалити разом з програмою (nullptr — немає)
    const wchar_t *deleteDataUk, *deleteDataEn;
    const wchar_t *keepUk, *keepEn;   // нагадування при видаленні
    bool hasThemeChoice;              // вибір світлої / темної теми в інсталяторі
    // реєстрація як програми для відкриття файлів (прибирається деінсталятором; nullptr — немає)
    const wchar_t *assocProgId;
    const wchar_t *const *assocExts;
    const wchar_t *registeredAppName;
    const wchar_t *const *cleanupKeys; // розділи HKCU, які видалити повністю
};

// Версія приходить з кореневого CMakeLists.txt (project(... VERSION x.y.z)) через згенерований version.h
#if __has_include("version.h")
#include "version.h"
#endif
#ifndef APP_VERSION_W
#define APP_VERSION_W L"0.0.0"
#endif

static const wchar_t *const kKillNames[] = {L"Lumio.exe", L"AlteScrinConnect.exe", nullptr};

static const AppInfo kApp = {
    L"Lumio",
    L"Lumio.exe",
    L"Lumio",
    L"Lumio",
    L"Lumio Portable",
    APP_VERSION_W,
    kKillNames,
    true,
    L"--tray",
    L"AlteScrinConnect",
    {{0xFF00, 0xC100, 0x4D00}, {0xFF00, 0x5F00, 0x8F00}, {0x6C00, 0x4B00, 0xFF00}},
    L"Скріншоти за мить.",
    L"Screenshots in an instant.",
    L"Lumio — скріншоти",
    L"Lumio — screenshots",
    L"Портативна версія: налаштування й знімки лежать в одній папці, її можна носити на флешці. "
    L"Нічого не записується в систему.",
    L"Portable version: settings and screenshots live in one folder that you can carry on a USB stick. "
    L"Nothing is written to the system.",
    "Lumio portable mode: settings and screenshots are stored next to Lumio.exe\r\n",
    false,
    L"Lumio\\Lumio.ini",
    L"Lumio.ini",
    L"Lumio",
    L"Видалити також налаштування Lumio?\n\nВаші знімки це не зачепить. Папку буде переміщено в кошик.",
    L"Also delete the Lumio settings?\n\nYour screenshots are not affected. The folder will be moved to the Recycle Bin.",
    L"Ваші знімки залишаться на місці.",
    L"Your screenshots will stay where they are.",
    false,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};
