# Lumio

🇺🇦 Українська · [🇬🇧 English](#english)

**Lumio** — відкритий скріншотер для Windows на **C++ / Qt**. Власна альтернатива Lightshot,
зроблена з нуля. Живе в треї, за кнопкою скріншота заморожує екран, дає виділити область,
намалювати поверх неї й одразу скопіювати чи зберегти. Усі знімки лежать локально у вашій галереї:
їх можна переглядати, копіювати, перейменовувати й видаляти.

Нічого не вивантажується в інтернет — усе зберігається тільки на вашому ПК. Без телеметрії й аналітики.
Єдиний мережевий запит — перевірка оновлень на GitHub, і лише коли ви самі її запускаєте.

**[⬇ Завантажити останню версію](https://github.com/coldtrace00/Lumio/releases/latest)** — файл `Lumio-Setup.exe`.

## Встановлення

Запустіть **`Lumio-Setup.exe`** — це один файл, у якому вже є вся програма. В інсталяторі можна вибрати:

- **Встановити на цей комп'ютер** у будь-яку папку (за замовчуванням `%LOCALAPPDATA%\Programs\Lumio`,
  права адміністратора не потрібні). Програма з'явиться в меню «Пуск» і в «Параметри → Програми»,
  звідки її можна видалити.
- **Портативна версія**: усе (налаштування й знімки) лежить в одній папці, її можна носити на флешці.
  У систему нічого не записується.
- Ярлик на робочому столі, автозапуск з Windows, запуск одразу після встановлення.
- **Мова (українська / English)** вибирається вгорі вікна інсталятора і передається програмі.

## Оновлення

**Однією кнопкою.** Правий клік по значку Lumio в треї → **«Перевірити й оновити…»**. Програма запитає GitHub
про останній реліз. Якщо є новіша версія, вона покаже список змін і кнопку **«Оновити зараз»**. Далі все
відбувається автоматично: програма завантажить інсталятор, звірить контрольну суму SHA-256 (якщо GitHub
її надає), встановить нову версію в ту саму папку й перезапуститься. Налаштування, знімки, ярлики,
автозапуск і портативний режим зберігаються.

**Вручну.** Завантажте `Lumio-Setup.exe` зі сторінки [Releases](https://github.com/coldtrace00/Lumio/releases/latest),
запустіть його і вкажіть ту саму папку, куди Lumio встановлено зараз (для звичайного встановлення це папка
за замовчуванням). Інсталятор сам закриє запущену програму, і нова версія встановиться поверх старої.
Знімки й налаштування не зачіпаються, лише мову буде взято з тієї, яку ви виберете в інсталяторі.

## Можливості

**Як у Lightshot**
- Значок у треї, глобальна клавіша (PrtSc або кнопка скріншота на клавіатурі)
- Затемнення екрана, виділення області мишею, розмір у пікселях
- Зміна розміру (8 маркерів) і переміщення виділення
- Олівець, лінія, стрілка, прямокутник, маркер, текст, вибір кольору
- Копіювати (Ctrl+C), зберегти (Ctrl+S), друк (Ctrl+P), скасування (Ctrl+Z)

**Понад Lightshot**
- **Галерея знімків**: мініатюри, перегляд, пошук, копіювання, «Зберегти як», перейменування,
  **видалення в кошик або назавжди**, перетягування файлів у провідник/месенджер
- **Інструмент «Вибір» (V)**: будь-який текст, стрілку чи фігуру можна **перетягнути**, видалити (Delete),
  перефарбувати чи змінити товщину. **Подвійний клік по тексту — редагувати**
- **Приховування інформації в 4 режимах**: пікселізація, розмиття, **розмиття з кольоровим тоном**,
  **заливка вибраним кольором** (маленька стрілка біля кнопки інструмента). Кожен новий знімок
  починається з пікселізації
- **Автозбереження** кожного скопійованого знімка в галерею (вимикається)
- **Розумний клік**: підсвічується вікно під курсором, один клік — знімок цього вікна
- **Лупа** з кольором пікселя (клавіша **C** копіює HEX-колір)
- Еліпс, нумерація кроків 1-2-3, рівні фігури через **Shift**, повтор (Ctrl+Y)
- **Закріпити знімок поверх усіх вікон** (Ctrl+T): тягати, масштабувати, змінювати прозорість
- Знімок усього екрана однією клавішею (Shift+PrtSc), знімок із затримкою 3/5/10 с
- **Перехоплює PrtSc і Win+Shift+S** — «Ножиці» Windows більше не заважають
- **Запис будь-якої клавіші** в налаштуваннях — працюють і спецкнопки клавіатур (Logitech MX Keys тощо)
- Кілька моніторів і масштаб екрана 125–200% (HiDPI): знімки в повній роздільній здатності
- **Дві мови: українська та англійська.** Вибір в інсталяторі, змінити можна в
  Налаштування → Загальні → «Мова / Language» (програма запропонує перезапуститися)

## Збирання з вихідного коду

Потрібно: **Qt 6.x** (або Qt 5.15) для **MinGW** + CMake (обидва ставляться через Qt Online Installer).

### Найпростіше — `build.bat`
Двічі клацніть **`build.bat`**. Скрипт сам знайде Qt у `C:\Qt` і створить:
- **`Lumio-Setup.exe`** — інсталятор в одному файлі;
- **`dist\Lumio.exe`** — готова програма з усіма DLL (можна запускати без встановлення).

Якщо Qt встановлено не в `C:\Qt`, відкрийте `build.bat` і вкажіть шляхи на початку файлу:
```bat
set "QT_DIR=D:\Qt\6.12.0\mingw_64"
set "MINGW_DIR=D:\Qt\Tools\mingw1310_64"
```

### Qt Creator
Відкрийте `CMakeLists.txt` (або `Lumio.pro`), виберіть кит **Desktop Qt … MinGW 64-bit**,
режим **Release** і натисніть **▶ Run**.

## Як випустити нову версію

1. Змініть версію в `CMakeLists.txt` (`project(Lumio VERSION 1.1.0 ...)`) і в `Lumio.pro` (`VERSION = 1.1.0`).
   Інсталятор бере версію з `CMakeLists.txt` автоматично.
2. Запустіть `build.bat`. З'явиться `Lumio-Setup.exe`.
3. На GitHub: **Releases → Draft a new release**, тег **`v1.1.0`**, опис змін, прикріпіть файл
   **`Lumio-Setup.exe`** (саме з такою назвою) і натисніть **Publish release**.

Програма порівнює тег останнього релізу зі своєю версією. Тож щойно реліз опубліковано,
пункт «Перевірити й оновити» запропонує його всім користувачам.

## Керування

| Де | Клавіша / дія | Що робить |
|---|---|---|
| Будь-де | **PrtSc** або **Win+Shift+S** | Виділити область |
| Будь-де | **Shift+PrtSc** | Весь екран одразу (зберегти + скопіювати) |
| Будь-де | **Ctrl+PrtSc** | Відкрити галерею |
| Трей | лівий / середній клік | Виділити область / галерея |
| Виділення | тягнути мишею | Виділити область |
| Виділення | клік | Знімок вікна під курсором (або всього екрана) |
| Виділення | Ctrl+A | Увесь екран |
| Виділення | **C** (до виділення) | Скопіювати колір пікселя під курсором |
| Редактор | **V** або Пробіл | Вибір: перетягувати фігури й текст |
| Редактор | P L A R E M T B N | Олівець, лінія, стрілка, прямокутник, еліпс, маркер, текст, приховування, нумерація |
| Редактор | Delete | Видалити вибрану фігуру |
| Редактор | подвійний клік по тексту | Редагувати текст |
| Редактор | стрілки | Посунути вибрану фігуру (або виділення); Shift — на 10 пікс. |
| Редактор | Alt+стрілки | Змінити розмір виділення |
| Редактор | колесо миші | Товщина / розмір тексту (і для вибраної фігури) |
| Редактор | Shift під час малювання | Квадрат, коло, лінії кратно 45° |
| Редактор | **Ctrl+C**, Enter, подвійний клік | Копіювати в буфер |
| Редактор | **Ctrl+S** / Ctrl+Shift+S | Зберегти в галерею / Зберегти як… |
| Редактор | Ctrl+T / Ctrl+P | Закріпити поверх вікон / друк |
| Редактор | Ctrl+Z / Ctrl+Y | Скасувати / повторити (зокрема переміщення) |
| Редактор | правий клік / Esc | Скинути виділення / вийти |
| Текст | Enter / Shift+Enter | Готово / новий рядок |
| Закріплений знімок | колесо / Ctrl+колесо | Масштаб / прозорість |
| Галерея | Ctrl+C, Delete, Shift+Delete, F2 | Копіювати, в кошик, назавжди, перейменувати |

Усі глобальні клавіші змінюються в **Налаштування → Гарячі клавіші**. Кнопка **«Записати…»**
запам'ятовує будь-яку натиснуту клавішу.

## Аргументи командного рядка
```
Lumio.exe --capture      виділити область
Lumio.exe --fullscreen   знімок усього екрана
Lumio.exe --gallery      відкрити галерею
Lumio.exe --settings     налаштування
Lumio.exe --tray         тихий запуск (так стартує автозапуск)
```

## Де що зберігається
- Знімки: `Зображення\Lumio` (у портативній версії — `Screenshots` поруч із програмою)
- Налаштування: `%APPDATA%\Lumio\Lumio.ini` (у портативній версії — поруч із програмою)
- Портативний режим вмикає порожній файл `portable.txt` поруч із `Lumio.exe`

## Структура проєкту
```
src/
  main.cpp             запуск, одна копія програми, аргументи
  updater.*            перевірка й встановлення оновлень з GitHub Releases
  i18n.*               українська / англійська
  app.*                трей, гарячі клавіші, запуск захоплення, обробка дій
  captureoverlay.*     повноекранне виділення + редактор + панелі інструментів
  annotation.*         фігури (олівець, стрілка, текст, розмиття...), малювання та вибір
  gallerywindow.*      галерея: мініатюри, перегляд, видалення, перейменування
  pinwindow.*          закріплений знімок поверх вікон
  settingsdialog.*     вікно налаштувань і запис гарячих клавіш
  hotkeymanager.*      глобальні клавіші (низькорівневий хук клавіатури WinAPI)
  platform.*           пошук вікон під курсором (WinAPI EnumWindows + DWM)
  imageio.*            збереження, буфер обміну, друк, кошик
  icons.*              лого й усі іконки, намальовані кодом
  settings.*           налаштування (QSettings)
installer/             інсталятор Lumio-Setup.exe і деінсталятор (Win32, без Qt)
third_party/miniz/     розпакування zip в інсталяторі (MIT)
resources/             іконка програми та ресурси .exe
```

## Ліцензія
MIT — див. файл [LICENSE](LICENSE). miniz — MIT (див. `third_party/miniz/LICENSE`).

---

<a id="english"></a>
# Lumio (English)

**Lumio** is an open-source screenshot tool for Windows written in **C++ / Qt** — your own Lightshot
alternative. It lives in the tray, freezes the screen when you press the screenshot key, lets you select an
area, draw on it and instantly copy or save it. All screenshots stay local in your gallery, where you can
view, copy, rename and delete them. Nothing is uploaded anywhere; no telemetry or analytics.
The only network request is the update check on GitHub, and only when you start it yourself.

**[⬇ Download the latest version](https://github.com/coldtrace00/Lumio/releases/latest)** — the `Lumio-Setup.exe` file.

## Installation
Run **`Lumio-Setup.exe`** — a single file containing the whole program. Pick the language
(Українська / English) at the top of the installer window, then choose:
- **Install on this computer** into any folder (no administrator rights) — Start menu, “Settings → Apps”;
- **Portable version** — everything (settings and screenshots) in one folder;
- desktop shortcut, start with Windows, launch after installation.

## Updates
**One button.** Right-click the Lumio tray icon → **“Check for updates…”**. The program asks GitHub for the
latest release. If a newer version exists, it shows what's new and an **“Update now”** button. The rest is
automatic: the program downloads the installer, verifies its SHA-256 checksum (when GitHub provides one),
installs the new version into the same folder and restarts. Settings, screenshots, shortcuts, autostart and
portable mode are kept.

**Manually.** Download `Lumio-Setup.exe` from the [Releases](https://github.com/coldtrace00/Lumio/releases/latest)
page, run it and choose the same folder Lumio is installed in now (for a regular install this is the default
folder). The installer closes the running program and installs the new version over the old one. Screenshots and
settings are not touched; only the language is taken from what you choose in the installer.

## Features
- Tray icon and a global hotkey: **PrtSc** or the screenshot key on your keyboard (e.g. Logitech MX Keys —
  both PrtSc and Win+Shift+S are captured, so the Windows Snipping Tool no longer gets in the way;
  any key can be recorded in the settings)
- Dimmed screen, area selection with size display, 8 resize handles, smart click to capture the window under the cursor
- Tools: pen, line, arrow, rectangle, ellipse, highlighter, text, step numbers, and **hiding information** in
  4 modes (pixelate, blur, tinted blur, solid color); every new capture starts with pixelate
- **Select tool (V)**: drag any text or shape, Delete to remove, double-click text to edit; undo/redo
- Copy (Ctrl+C), save to gallery (Ctrl+S), save as, pin on top of all windows (Ctrl+T), print (Ctrl+P)
- Magnifier with pixel color (press **C** to copy the HEX color)
- **Gallery** with thumbnails, preview, search, copy, rename, delete to the Recycle Bin or permanently
- Full-screen capture (Shift+PrtSc), delayed capture, multiple monitors and HiDPI
- **Two languages: Ukrainian and English** — chosen in the installer, changeable in
  Settings → General → “Мова / Language”

## Building from source
Requires **Qt 6.x** (or 5.15) for **MinGW** + CMake. Double-click **`build.bat`** — it finds Qt in `C:\Qt` and
creates **`Lumio-Setup.exe`** (one-file installer) and **`dist\Lumio.exe`** (ready-to-run program).

## Publishing a new version
1. Change the version in `CMakeLists.txt` (`project(Lumio VERSION 1.1.0 ...)`) and in `Lumio.pro` (`VERSION = 1.1.0`).
   The installer picks the version up from `CMakeLists.txt` automatically.
2. Run `build.bat`. It produces `Lumio-Setup.exe`.
3. On GitHub: **Releases → Draft a new release**, tag **`v1.1.0`**, describe the changes, attach
   **`Lumio-Setup.exe`** (exactly this name) and click **Publish release**.

The program compares the latest release tag with its own version, so as soon as the release is published,
“Check for updates” offers it to every user.

## License
MIT — see [LICENSE](LICENSE). miniz — MIT (`third_party/miniz/LICENSE`).
