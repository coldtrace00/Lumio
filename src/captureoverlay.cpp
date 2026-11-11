#include "captureoverlay.h"
#include "i18n.h"

#include "compat.h"
#include "icons.h"
#include "platform.h"
#include "settings.h"

#include <QBoxLayout>
#include <QClipboard>
#include <QColorDialog>
#include <QCursor>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineF>
#include <QMenu>
#include <QPainter>
#include <QRegion>
#include <QScreen>
#include <QToolButton>
#include <QWidgetAction>
#include <QWindow>

#include <cmath>
#include <utility>

namespace {

constexpr double kPi = 3.14159265358979323846;
const QColor kAccent(0x2f, 0x8c, 0xff);

const char *kPanelStyle = R"(
QFrame#panel {
    background: rgba(32, 33, 36, 238);
    border: 1px solid rgba(255, 255, 255, 45);
    border-radius: 8px;
}
QFrame#sep { background: rgba(255, 255, 255, 45); border: none; }
QToolButton {
    background: transparent;
    border: none;
    border-radius: 6px;
    padding: 5px;
}
QToolButton:hover   { background: rgba(255, 255, 255, 32); }
QToolButton:pressed { background: rgba(255, 255, 255, 60); }
QToolButton:checked { background: #2f6fd6; }
QToolButton:disabled { background: transparent; }
QToolButton[popupMode="1"] { padding-right: 12px; }
QToolButton::menu-button { border: none; width: 11px; }
QToolButton::menu-arrow { width: 7px; height: 7px; }
QToolTip {
    color: #ffffff;
    background: #202124;
    border: 1px solid #555555;
    padding: 4px;
}
)";

const char *kMenuStyle = R"(
QMenu { background: #202124; color: #ffffff; border: 1px solid #555555; padding: 4px; }
QMenu::item { padding: 6px 18px; border-radius: 4px; }
QMenu::item:selected { background: #2f6fd6; }
QMenu::separator { height: 1px; background: #444444; margin: 4px 6px; }
QToolButton { border: none; border-radius: 4px; padding: 2px; }
QToolButton:hover { background: rgba(255,255,255,40); }
)";

struct ToolDef {
    Tool tool;
    Icons::Id icon;
    const char *tip;
    const char *tipEn;
};

const ToolDef kTools[] = {
    {Tool::Pen,      Icons::Pen,      "Олівець (P)", "Pen (P)"},
    {Tool::Line,     Icons::Line,     "Лінія (L)\nShift — кратно 45°", "Line (L)\nShift — snap to 45°"},
    {Tool::Arrow,    Icons::Arrow,    "Стрілка (A)\nShift — кратно 45°", "Arrow (A)\nShift — snap to 45°"},
    {Tool::Rect,     Icons::Rect,     "Прямокутник (R)\nShift — квадрат", "Rectangle (R)\nShift — square"},
    {Tool::Ellipse,  Icons::Ellipse,  "Еліпс (E)\nShift — коло", "Ellipse (E)\nShift — circle"},
    {Tool::Marker,   Icons::Marker,   "Маркер (M)", "Highlighter (M)"},
    {Tool::Text,     Icons::Text,     "Текст (T)\nEnter — готово, Shift+Enter — новий рядок", "Text (T)\nEnter — done, Shift+Enter — new line"},
    {Tool::Pixelate, Icons::Pixelate, "Приховати інформацію (B)\n▸ стрілка праворуч — пікселі / розмиття / колір\nКолір береться з кнопки «Колір»", "Hide information (B)\n▸ arrow on the right — pixels / blur / color\nThe color comes from the “Color” button"},
    {Tool::Counter,  Icons::Counter,  "Нумерація кроків (N)", "Step numbers (N)"},
};

QRect makeRect(const QPoint &a, const QPoint &b)
{
    return QRect(QPoint(qMin(a.x(), b.x()), qMin(a.y(), b.y())),
                 QPoint(qMax(a.x(), b.x()), qMax(a.y(), b.y())));
}

} // namespace

CaptureOverlay::CaptureOverlay(QScreen *screen, const QPixmap &shot, const QVector<QRect> &windows, QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
    , m_screen(screen)
    , m_shot(shot)
    , m_windows(windows)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);
    setWindowTitle(QStringLiteral("Lumio"));

    m_dpr = m_shot.devicePixelRatio();
    m_base = m_shot.toImage();
    m_base.setDevicePixelRatio(1.0);

    m_color = Settings::penColor();
    m_obscureStyle = ObscurePixelate; // кожен новий знімок — з пікселізації за замовчуванням
    m_width = Settings::penWidth();
    m_showMagnifier = Settings::showMagnifier();

    m_caretTimer.setInterval(530);
    connect(&m_caretTimer, &QTimer::timeout, this, [this]() {
        m_caretOn = !m_caretOn;
        update();
    });
    m_hintTimer.setSingleShot(true);
    m_hintTimer.setInterval(900);
    connect(&m_hintTimer, &QTimer::timeout, this, [this]() {
        m_showWidthHint = false;
        update();
    });

    buildPanels();
}

void CaptureOverlay::showOnScreen()
{
    const QRect g = m_screen->geometry();
    setGeometry(g);
    winId(); // створює нативне вікно, щоб можна було прив'язати його до екрана
    if (windowHandle())
        windowHandle()->setScreen(m_screen);
    setGeometry(g);
    show();
    m_mouse = mapFromGlobal(QCursor::pos());
    m_mouseInside = rect().contains(m_mouse);
    updateHover();
}

void CaptureOverlay::activate()
{
    raise();
    activateWindow();
    Platform::forceForeground(this);
    setFocus(Qt::ActiveWindowFocusReason);
}

// ---------------------------------------------------------------------------
// Панелі інструментів

QToolButton *CaptureOverlay::addButton(QFrame *panel, int icon, const QString &tip)
{
    auto *b = new QToolButton(panel);
    b->setIcon(Icons::icon(Icons::Id(icon)));
    b->setIconSize(QSize(20, 20));
    b->setToolTip(tip);
    b->setFocusPolicy(Qt::NoFocus);
    b->setAutoRaise(true);
    panel->layout()->addWidget(b);
    return b;
}

void CaptureOverlay::addSeparator(QFrame *panel, bool vertical)
{
    auto *s = new QFrame(panel);
    s->setObjectName(QStringLiteral("sep"));
    if (vertical)
        s->setFixedHeight(1);
    else
        s->setFixedWidth(1);
    panel->layout()->addWidget(s);
}

void CaptureOverlay::buildPanels()
{
    auto makePanel = [this](bool vertical) {
        auto *f = new QFrame(this);
        f->setObjectName(QStringLiteral("panel"));
        f->setStyleSheet(QString::fromUtf8(kPanelStyle));
        f->setCursor(Qt::ArrowCursor);
        QBoxLayout *l = vertical ? static_cast<QBoxLayout *>(new QVBoxLayout(f))
                                 : static_cast<QBoxLayout *>(new QHBoxLayout(f));
        l->setContentsMargins(4, 4, 4, 4);
        l->setSpacing(2);
        f->hide();
        return f;
    };

    // --- вертикальна панель: інструменти малювання
    m_toolPanel = makePanel(true);
    {
        QToolButton *cur = addButton(m_toolPanel, Icons::Cursor,
                                     tr2("Вибір і переміщення (V)\n"
                                                    "Перетягуйте фігури й текст, Delete — видалити,\n"
                                                    "подвійний клік по тексту — редагувати", "Select and move (V)\nDrag shapes and text, Delete — remove,\ndouble-click text — edit"));
        cur->setCheckable(true);
        cur->setChecked(true);
        connect(cur, &QToolButton::clicked, this, [this]() { setTool(Tool::None); });
        m_toolButtons.insert(Tool::None, cur);
        addSeparator(m_toolPanel, true);
    }
    for (const ToolDef &d : kTools) {
        QToolButton *b = addButton(m_toolPanel, d.icon,
                                   tr2(d.tip, d.tipEn) + tr2("\nКолесо миші — товщина", "\nMouse wheel — thickness"));
        b->setCheckable(true);
        const Tool t = d.tool;
        connect(b, &QToolButton::clicked, this, [this, t]() { toggleTool(t); });
        m_toolButtons.insert(t, b);
        if (t == Tool::Pixelate) {
            m_obscureButton = b;
            auto *menu = new QMenu(b);
            menu->setStyleSheet(QString::fromUtf8(kMenuStyle));
            const struct { int style; const char *text; const char *textEn; Icons::Id icon; } styles[] = {
                {ObscurePixelate, "Пікселізація", "Pixelate", Icons::Pixelate},
                {ObscureBlur, "Розмиття", "Blur", Icons::Blur},
                {ObscureTintedBlur, "Розмиття + колір", "Blur + color", Icons::Blur},
                {ObscureSolid, "Заливка кольором", "Solid color", Icons::Rect},
            };
            for (const auto &st : styles) {
                QAction *a = menu->addAction(Icons::icon(st.icon), tr2(st.text, st.textEn));
                a->setCheckable(true);
                a->setData(st.style);
                const int style = st.style;
                connect(a, &QAction::triggered, this, [this, style]() { setObscureStyle(style); });
            }
            connect(menu, &QMenu::aboutToShow, menu, [this, menu]() {
                for (QAction *a : menu->actions())
                    a->setChecked(a->data().toInt() == m_obscureStyle);
            });
            b->setMenu(menu);
            b->setPopupMode(QToolButton::MenuButtonPopup);
            b->setIcon(Icons::icon(m_obscureStyle == ObscurePixelate ? Icons::Pixelate
                                   : m_obscureStyle == ObscureSolid  ? Icons::Rect
                                                                     : Icons::Blur));
        }
    }
    addSeparator(m_toolPanel, true);
    m_colorButton = addButton(m_toolPanel, Icons::Pen, tr2("Колір", "Color"));
    m_colorButton->setIcon(Icons::colorSwatch(m_color));
    connect(m_colorButton, &QToolButton::clicked, this, &CaptureOverlay::chooseColor);
    m_undoButton = addButton(m_toolPanel, Icons::Undo, tr2("Скасувати (Ctrl+Z)", "Undo (Ctrl+Z)"));
    connect(m_undoButton, &QToolButton::clicked, this, &CaptureOverlay::undo);
    m_redoButton = addButton(m_toolPanel, Icons::Redo, tr2("Повторити (Ctrl+Y)", "Redo (Ctrl+Y)"));
    connect(m_redoButton, &QToolButton::clicked, this, &CaptureOverlay::redo);

    // --- горизонтальна панель: дії
    m_actionPanel = makePanel(false);
    QToolButton *b = addButton(m_actionPanel, Icons::Copy, tr2("Копіювати в буфер (Ctrl+C / Enter)", "Copy to clipboard (Ctrl+C / Enter)"));
    connect(b, &QToolButton::clicked, this, [this]() { finish(Action::Copy); });
    b = addButton(m_actionPanel, Icons::Save, tr2("Зберегти в галерею (Ctrl+S)", "Save to gallery (Ctrl+S)"));
    connect(b, &QToolButton::clicked, this, [this]() { finish(Action::Save); });
    b = addButton(m_actionPanel, Icons::SaveAs, tr2("Зберегти як… (Ctrl+Shift+S)", "Save as… (Ctrl+Shift+S)"));
    connect(b, &QToolButton::clicked, this, [this]() { finish(Action::SaveAs); });
    b = addButton(m_actionPanel, Icons::Pin, tr2("Закріпити поверх усіх вікон (Ctrl+T)", "Pin on top of all windows (Ctrl+T)"));
    connect(b, &QToolButton::clicked, this, [this]() { finish(Action::Pin); });
    b = addButton(m_actionPanel, Icons::Print, tr2("Друк (Ctrl+P)", "Print (Ctrl+P)"));
    connect(b, &QToolButton::clicked, this, [this]() { finish(Action::Print); });
    addSeparator(m_actionPanel, false);
    b = addButton(m_actionPanel, Icons::Close, tr2("Закрити (Esc)", "Close (Esc)"));
    connect(b, &QToolButton::clicked, this, &CaptureOverlay::cancelled);

    updateUndoButtons();
}

void CaptureOverlay::updatePanels()
{
    const bool show = m_hasSel && (m_drag == Drag::None || m_drag == Drag::Drawing);
    m_toolPanel->setVisible(show);
    m_actionPanel->setVisible(show);
    if (!show)
        return;

    const QSize ts = m_toolPanel->sizeHint();
    const QSize as = m_actionPanel->sizeHint();
    const int gap = 8;

    // Інструменти — праворуч від виділення (або ліворуч / всередині)
    QPoint tp(m_sel.right() + gap + 1, m_sel.top());
    if (tp.x() + ts.width() > width())
        tp.setX(m_sel.left() - gap - ts.width());
    if (tp.x() < 0)
        tp.setX(m_sel.right() - ts.width() - gap);
    tp.setX(qBound(0, tp.x(), qMax(0, width() - ts.width())));
    tp.setY(qBound(0, tp.y(), qMax(0, height() - ts.height())));

    // Дії — під виділенням, вирівняні по правому краю (або над / всередині)
    QPoint ap(m_sel.right() - as.width() + 1, m_sel.bottom() + gap + 1);
    if (ap.y() + as.height() > height())
        ap.setY(m_sel.top() - gap - as.height());
    if (ap.y() < 0)
        ap.setY(m_sel.bottom() - as.height() - gap);
    ap.setX(qBound(0, ap.x(), qMax(0, width() - as.width())));
    ap.setY(qBound(0, ap.y(), qMax(0, height() - as.height())));

    const QRect tr(tp, ts);
    QRect ar(ap, as);
    if (tr.intersects(ar)) {
        if (tr.left() >= ar.left())
            ar.moveRight(tr.left() - gap);
        else
            ar.moveLeft(tr.right() + gap);
        if (ar.left() < 0 || ar.right() >= width())
            ar.moveTo(qBound(0, ar.left(), qMax(0, width() - as.width())), qMax(0, tr.bottom() + gap));
    }

    m_toolPanel->setGeometry(tr);
    m_actionPanel->setGeometry(ar);
    m_toolPanel->raise();
    m_actionPanel->raise();
}

void CaptureOverlay::updateUndoButtons()
{
    if (m_undoButton)
        m_undoButton->setEnabled(!m_undoStack.isEmpty());
    if (m_redoButton)
        m_redoButton->setEnabled(!m_redoStack.isEmpty());
}

void CaptureOverlay::setTool(Tool t)
{
    commitText();
    m_tool = t;
    if (t != Tool::None)
        m_selectedShape = -1;
    for (auto it = m_toolButtons.begin(); it != m_toolButtons.end(); ++it)
        it.value()->setChecked(it.key() == t);
    updateCursor(m_mouse);
    update();
}

void CaptureOverlay::toggleTool(Tool t)
{
    setTool(m_tool == t ? Tool::None : t);
}

void CaptureOverlay::chooseColor()
{
    commitText();
    QMenu menu(this);
    menu.setStyleSheet(QString::fromUtf8(kMenuStyle));

    static const QList<QColor> colors = {
        QColor(0xff, 0x3b, 0x30), QColor(0xff, 0x95, 0x00), QColor(0xff, 0xcc, 0x00), QColor(0x34, 0xc7, 0x59),
        QColor(0x32, 0xad, 0xe6), QColor(0x00, 0x7a, 0xff), QColor(0xaf, 0x52, 0xde), QColor(0xff, 0x2d, 0x55),
        QColor(0xff, 0xff, 0xff), QColor(0x8e, 0x8e, 0x93), QColor(0x3a, 0x3a, 0x3c), QColor(0x00, 0x00, 0x00),
    };

    auto *grid = new QWidget(&menu);
    auto *gl = new QGridLayout(grid);
    gl->setContentsMargins(6, 6, 6, 6);
    gl->setSpacing(4);
    for (int i = 0; i < colors.size(); ++i) {
        auto *btn = new QToolButton(grid);
        btn->setIcon(Icons::colorSwatch(colors[i], 22));
        btn->setIconSize(QSize(22, 22));
        btn->setFocusPolicy(Qt::NoFocus);
        const QColor c = colors[i];
        connect(btn, &QToolButton::clicked, &menu, [this, c, &menu]() {
            setColor(c);
            menu.close();
        });
        gl->addWidget(btn, i / 4, i % 4);
    }
    auto *wa = new QWidgetAction(&menu);
    wa->setDefaultWidget(grid);
    menu.addAction(wa);
    menu.addSeparator();
    QAction *custom = menu.addAction(tr2("Інший колір…", "Other color…"));

    const QPoint at = m_colorButton->mapToGlobal(QPoint(m_colorButton->width() + 6, 0));
    if (menu.exec(at) == custom) {
        const QColor c = QColorDialog::getColor(m_color, this, tr2("Колір", "Color"), QColorDialog::ShowAlphaChannel);
        if (c.isValid())
            setColor(c);
    }
    activate();
}

void CaptureOverlay::setColor(const QColor &c)
{
    m_color = c;
    m_colorButton->setIcon(Icons::colorSwatch(c));
    if (m_textIndex >= 0 && m_textIndex < m_shapes.size()) {
        m_shapes[m_textIndex].color = c;
    } else if (hasSelectedShape()) {
        pushUndo();
        m_shapes[m_selectedShape].color = c;
    }
    Settings::setValue(QStringLiteral("editor/color"), c.name(QColor::HexArgb));
    update();
}

void CaptureOverlay::setPenWidth(int w)
{
    m_width = qBound(1, w, 30);
    Settings::setValue(QStringLiteral("editor/width"), m_width);
    if (m_textIndex >= 0 && m_textIndex < m_shapes.size()) {
        m_shapes[m_textIndex].width = m_width;
    } else if (hasSelectedShape()) {
        pushUndo();
        m_shapes[m_selectedShape].width = m_width;
    }
    m_showWidthHint = true;
    m_hintTimer.start();
    update();
}

// ---------------------------------------------------------------------------
// Дії

QImage CaptureOverlay::renderResult() const
{
    QRect phys(qRound(m_sel.x() * m_dpr), qRound(m_sel.y() * m_dpr),
               qRound(m_sel.width() * m_dpr), qRound(m_sel.height() * m_dpr));
    phys = phys.intersected(m_base.rect());
    if (phys.isEmpty())
        return QImage();

    QImage img = m_base.copy(phys).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        p.translate(-phys.x(), -phys.y());
        p.scale(m_dpr, m_dpr);
        for (const Shape &s : m_shapes)
            Annotation::draw(p, s, m_base, m_dpr);
    }
    img = img.convertToFormat(QImage::Format_RGB32);
    img.setDevicePixelRatio(m_dpr);
    return img;
}

void CaptureOverlay::finish(Action a)
{
    if (!m_hasSel)
        return;
    commitText();
    const QImage img = renderResult();
    if (img.isNull())
        return;
    const QRect global(mapToGlobal(m_sel.topLeft()), m_sel.size());
    emit actionRequested(a, img, global);
}

void CaptureOverlay::pushUndo()
{
    m_undoStack.append(m_shapes);
    if (m_undoStack.size() > 300)
        m_undoStack.removeFirst();
    m_redoStack.clear();
    updateUndoButtons();
}

void CaptureOverlay::undo()
{
    commitText();
    if (m_undoStack.isEmpty())
        return;
    m_redoStack.append(m_shapes);
    m_shapes = m_undoStack.takeLast();
    m_selectedShape = -1;
    updateUndoButtons();
    update();
}

void CaptureOverlay::redo()
{
    commitText();
    if (m_redoStack.isEmpty())
        return;
    m_undoStack.append(m_shapes);
    m_shapes = m_redoStack.takeLast();
    m_selectedShape = -1;
    updateUndoButtons();
    update();
}

void CaptureOverlay::commitText()
{
    if (m_textIndex < 0)
        return;
    if (m_textIndex < m_shapes.size()) {
        const Shape &s = m_shapes[m_textIndex];
        if (s.text.trimmed().isEmpty()) {
            m_shapes.removeAt(m_textIndex);
            if (m_textIsNew && !m_undoStack.isEmpty())
                m_undoStack.removeLast(); // порожній новий текст — ніби нічого не було
            m_selectedShape = -1;
        } else if (!m_textIsNew && s.text == m_textOriginal && !m_undoStack.isEmpty()) {
            m_undoStack.removeLast(); // нічого не змінили
        }
    }
    m_textIndex = -1;
    m_caretTimer.stop();
    updateUndoButtons();
    update();
}

void CaptureOverlay::editText(int index)
{
    commitText();
    if (index < 0 || index >= m_shapes.size() || m_shapes[index].tool != Tool::Text)
        return;
    pushUndo();
    m_textIndex = index;
    m_textIsNew = false;
    m_textOriginal = m_shapes[index].text;
    m_selectedShape = index;
    m_caretOn = true;
    m_caretTimer.start();
    update();
}

int CaptureOverlay::shapeAt(const QPoint &pos) const
{
    for (int i = m_shapes.size() - 1; i >= 0; --i)
        if (Annotation::hitTest(m_shapes[i], QPointF(pos)))
            return i;
    return -1;
}

void CaptureOverlay::deleteSelectedShape()
{
    if (!hasSelectedShape())
        return;
    pushUndo();
    m_shapes.removeAt(m_selectedShape);
    m_selectedShape = -1;
    update();
}

void CaptureOverlay::setObscureStyle(int style)
{
    m_obscureStyle = qBound(0, style, 3);
    if (m_obscureButton)
        m_obscureButton->setIcon(Icons::icon(m_obscureStyle == ObscurePixelate ? Icons::Pixelate
                                             : m_obscureStyle == ObscureSolid  ? Icons::Rect
                                                                               : Icons::Blur));
    if (hasSelectedShape() && m_shapes[m_selectedShape].tool == Tool::Pixelate) {
        pushUndo();
        m_shapes[m_selectedShape].style = m_obscureStyle;
        update();
        return;
    }
    setTool(Tool::Pixelate);
}

int CaptureOverlay::nextCounterNumber() const
{
    int n = 0;
    for (const Shape &s : m_shapes)
        if (s.tool == Tool::Counter)
            n = qMax(n, s.number);
    return n + 1;
}

void CaptureOverlay::startShape(const QPoint &pos)
{
    pushUndo();
    m_selectedShape = -1;
    Shape s;
    s.tool = m_tool;
    s.color = m_color;
    s.width = m_width;
    s.style = m_obscureStyle;
    s.points = {QPointF(pos)};

    switch (m_tool) {
    case Tool::Text: {
        const QFontMetricsF fm(Annotation::textFont(m_width));
        s.points = {QPointF(pos.x(), pos.y() - fm.height() / 2.0)};
        m_shapes.append(s);
        m_textIndex = m_shapes.size() - 1;
        m_textIsNew = true;
        m_caretOn = true;
        m_caretTimer.start();
        break;
    }
    case Tool::Counter:
        s.number = nextCounterNumber();
        m_shapes.append(s);
        break;
    case Tool::Pen:
    case Tool::Marker:
        m_shapes.append(s);
        m_drag = Drag::Drawing;
        break;
    default:
        s.points << QPointF(pos);
        m_shapes.append(s);
        m_drag = Drag::Drawing;
        break;
    }
    updateUndoButtons();
    update();
}

QPointF CaptureOverlay::constrained(const QPointF &from, const QPointF &to, Tool t) const
{
    const QPointF d = to - from;
    if (t == Tool::Line || t == Tool::Arrow) {
        const double step = kPi / 4.0;
        const double ang = std::round(std::atan2(d.y(), d.x()) / step) * step;
        const double len = std::hypot(d.x(), d.y());
        return from + QPointF(std::cos(ang) * len, std::sin(ang) * len);
    }
    const qreal s = qMax(qAbs(d.x()), qAbs(d.y()));
    return from + QPointF(d.x() < 0 ? -s : s, d.y() < 0 ? -s : s);
}

void CaptureOverlay::selectAll()
{
    commitText();
    m_sel = rect();
    m_hasSel = true;
    m_hoverWindow = QRect();
    updatePanels();
    update();
}

void CaptureOverlay::resetSelection()
{
    commitText();
    m_hasSel = false;
    m_sel = QRect();
    m_shapes.clear();
    m_undoStack.clear();
    m_redoStack.clear();
    m_selectedShape = -1;
    m_drag = Drag::None;
    updatePanels();
    updateUndoButtons();
    updateHover();
    updateCursor(m_mouse);
    update();
}

void CaptureOverlay::moveSelection(int dx, int dy)
{
    QRect r = m_sel.translated(dx, dy);
    if (r.left() < 0) r.moveLeft(0);
    if (r.top() < 0) r.moveTop(0);
    if (r.right() > width() - 1) r.moveRight(width() - 1);
    if (r.bottom() > height() - 1) r.moveBottom(height() - 1);
    m_sel = r;
    updatePanels();
    update();
}

void CaptureOverlay::resizeSelection(int dx, int dy)
{
    QRect r = m_sel;
    r.setWidth(qMax(1, r.width() + dx));
    r.setHeight(qMax(1, r.height() + dy));
    m_sel = r.intersected(rect());
    updatePanels();
    update();
}

void CaptureOverlay::copyColorUnderCursor()
{
    const QPoint phys(int(std::floor(m_mouse.x() * m_dpr)), int(std::floor(m_mouse.y() * m_dpr)));
    if (!m_base.valid(phys))
        return;
    QGuiApplication::clipboard()->setText(m_base.pixelColor(phys).name().toUpper());
    emit cancelled();
}

// ---------------------------------------------------------------------------
// Миша / клавіатура

Qt::Edges CaptureOverlay::edgesAt(const QPoint &pt) const
{
    if (!m_hasSel)
        return Qt::Edges();
    const int m = 6;
    const QRect r = m_sel;
    const bool inY = pt.y() >= r.top() - m && pt.y() <= r.bottom() + m;
    const bool inX = pt.x() >= r.left() - m && pt.x() <= r.right() + m;
    Qt::Edges e;
    if (inY && qAbs(pt.x() - r.left()) <= m)
        e |= Qt::LeftEdge;
    else if (inY && qAbs(pt.x() - r.right()) <= m)
        e |= Qt::RightEdge;
    if (inX && qAbs(pt.y() - r.top()) <= m)
        e |= Qt::TopEdge;
    else if (inX && qAbs(pt.y() - r.bottom()) <= m)
        e |= Qt::BottomEdge;
    return e;
}

void CaptureOverlay::updateHover()
{
    m_hoverWindow = QRect();
    if (m_hasSel || m_drag != Drag::None)
        return;
    for (const QRect &r : std::as_const(m_windows)) {
        if (r.contains(m_mouse)) {
            m_hoverWindow = r.intersected(rect());
            break;
        }
    }
}

void CaptureOverlay::updateCursor(const QPoint &pos)
{
    if (!m_hasSel) {
        setCursor(Qt::CrossCursor);
        return;
    }
    const Qt::Edges e = edgesAt(pos);
    if (e == (Qt::LeftEdge | Qt::TopEdge) || e == (Qt::RightEdge | Qt::BottomEdge))
        setCursor(Qt::SizeFDiagCursor);
    else if (e == (Qt::RightEdge | Qt::TopEdge) || e == (Qt::LeftEdge | Qt::BottomEdge))
        setCursor(Qt::SizeBDiagCursor);
    else if (e == Qt::LeftEdge || e == Qt::RightEdge)
        setCursor(Qt::SizeHorCursor);
    else if (e == Qt::TopEdge || e == Qt::BottomEdge)
        setCursor(Qt::SizeVerCursor);
    else if (m_sel.contains(pos)) {
        const int hit = shapeAt(pos);
        if (hit >= 0 && (m_tool == Tool::None || (m_tool == Tool::Text && m_shapes[hit].tool == Tool::Text)))
            setCursor(Qt::PointingHandCursor);
        else if (m_tool == Tool::None)
            setCursor(Qt::SizeAllCursor);
        else if (m_tool == Tool::Text)
            setCursor(Qt::IBeamCursor);
        else
            setCursor(Qt::CrossCursor);
    } else {
        setCursor(m_tool == Tool::None ? Qt::CrossCursor : Qt::ArrowCursor);
    }
}

void CaptureOverlay::mousePressEvent(QMouseEvent *e)
{
    const QPoint pos = Compat::pos(e);
    m_mouse = pos;

    if (e->button() == Qt::RightButton) {
        if (m_textIndex >= 0)
            commitText();
        else if (m_hasSel)
            resetSelection();
        else
            emit cancelled();
        return;
    }
    if (e->button() != Qt::LeftButton)
        return;

    if (m_textIndex >= 0)
        commitText();

    m_pressPos = pos;
    m_selAtPress = m_sel;

    if (!m_hasSel) {
        m_drag = Drag::Selecting;
        m_sel = QRect(pos, QSize(1, 1));
        update();
        return;
    }

    const Qt::Edges edges = edgesAt(pos);
    if (edges) {
        m_drag = Drag::Resizing;
        m_edges = edges;
        updatePanels();
        return;
    }

    // Клік по фігурі: вибрати й тягнути (у режимі вибору, або текст — інструментом «Текст»)
    const int hit = m_sel.contains(pos) ? shapeAt(pos) : -1;
    if (hit >= 0 && (m_tool == Tool::None || (m_tool == Tool::Text && m_shapes[hit].tool == Tool::Text))) {
        m_selectedShape = hit;
        m_shapeAtPress = m_shapes[hit];
        m_shapeMoved = false;
        m_drag = Drag::MovingShape;
        setCursor(Qt::ClosedHandCursor);
        update();
        return;
    }

    if (m_tool != Tool::None) {
        if (m_sel.contains(pos))
            startShape(pos);
        return;
    }

    m_selectedShape = -1;
    if (m_sel.contains(pos)) {
        m_drag = Drag::Moving;
        updatePanels();
        return;
    }

    // Клік поза виділенням без інструмента — нове виділення
    m_hasSel = false;
    m_drag = Drag::Selecting;
    m_sel = QRect(pos, QSize(1, 1));
    updatePanels();
    update();
}

void CaptureOverlay::mouseMoveEvent(QMouseEvent *e)
{
    const QPoint pos = Compat::pos(e);
    m_mouse = pos;
    m_mouseInside = true;

    switch (m_drag) {
    case Drag::Selecting:
        m_sel = makeRect(m_pressPos, pos).intersected(rect());
        break;
    case Drag::Moving: {
        const QPoint d = pos - m_pressPos;
        m_sel = m_selAtPress;
        moveSelection(d.x(), d.y());
        break;
    }
    case Drag::Resizing: {
        QRect r = m_selAtPress;
        const QPoint d = pos - m_pressPos;
        if (m_edges & Qt::LeftEdge)   r.setLeft(r.left() + d.x());
        if (m_edges & Qt::RightEdge)  r.setRight(r.right() + d.x());
        if (m_edges & Qt::TopEdge)    r.setTop(r.top() + d.y());
        if (m_edges & Qt::BottomEdge) r.setBottom(r.bottom() + d.y());
        r = r.normalized().intersected(rect());
        if (r.isEmpty())
            r = QRect(pos, QSize(1, 1));
        m_sel = r;
        break;
    }
    case Drag::Drawing: {
        if (m_shapes.isEmpty())
            break;
        Shape &s = m_shapes.last();
        const QPointF p(pos);
        if (s.tool == Tool::Pen || s.tool == Tool::Marker) {
            if (QLineF(s.points.last(), p).length() >= 1.0)
                s.points.append(p);
        } else if (s.points.size() >= 2) {
            s.points[1] = (e->modifiers() & Qt::ShiftModifier) ? constrained(s.points[0], p, s.tool) : p;
        }
        break;
    }
    case Drag::MovingShape: {
        if (!hasSelectedShape())
            break;
        const QPoint d = pos - m_pressPos;
        if (!m_shapeMoved) {
            if (d.manhattanLength() < 3)
                break;
            pushUndo(); // знімок «до» переміщення
            m_shapeMoved = true;
        }
        Shape moved = m_shapeAtPress;
        Annotation::translate(moved, QPointF(d));
        m_shapes[m_selectedShape] = moved;
        break;
    }
    case Drag::None:
        updateHover();
        updateCursor(pos);
        break;
    }
    update();
}

void CaptureOverlay::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    const QPoint pos = Compat::pos(e);
    const Drag d = m_drag;
    m_drag = Drag::None;

    if (d == Drag::Selecting) {
        if ((pos - m_pressPos).manhattanLength() < 4) {
            // Простий клік: вибрати вікно під курсором або весь екран
            m_sel = m_hoverWindow.isEmpty() ? rect() : m_hoverWindow;
        }
        m_hasSel = !m_sel.isEmpty();
        m_hoverWindow = QRect();
    } else if (d == Drag::MovingShape) {
        // Простий клік по тексту інструментом «Текст» — редагувати його
        if (!m_shapeMoved && m_tool == Tool::Text && hasSelectedShape()
            && m_shapes[m_selectedShape].tool == Tool::Text)
            editText(m_selectedShape);
    } else if (d == Drag::Drawing && !m_shapes.isEmpty()) {
        const Shape &s = m_shapes.last();
        if (s.tool != Tool::Pen && s.tool != Tool::Marker && s.points.size() >= 2
            && QLineF(s.points[0], s.points[1]).length() < 3) {
            m_shapes.removeLast(); // випадковий клік — не малюємо «точку»
            if (!m_undoStack.isEmpty())
                m_undoStack.removeLast();
        }
    }

    updatePanels();
    updateUndoButtons();
    updateCursor(pos);
    update();
}

void CaptureOverlay::mouseDoubleClickEvent(QMouseEvent *e)
{
    const QPoint pos = Compat::pos(e);
    if (e->button() != Qt::LeftButton || !m_hasSel || !m_sel.contains(pos))
        return;
    const int hit = shapeAt(pos);
    if (hit >= 0) {
        m_drag = Drag::None;
        if (m_shapes[hit].tool == Tool::Text)
            editText(hit); // подвійний клік по тексту — редагувати
        return;
    }
    if (m_tool == Tool::None) {
        m_drag = Drag::None;
        finish(Action::Copy);
    }
}

void CaptureOverlay::wheelEvent(QWheelEvent *e)
{
    const int delta = e->angleDelta().y();
    if (delta == 0)
        return;
    m_mouse = Compat::pos(e);
    setPenWidth(m_width + (delta > 0 ? 1 : -1));
}

void CaptureOverlay::leaveEvent(QEvent *e)
{
    m_mouseInside = false;
    QWidget::leaveEvent(e);
    update();
}

void CaptureOverlay::keyPressEvent(QKeyEvent *e)
{
    const bool ctrl = e->modifiers() & Qt::ControlModifier;
    const bool shift = e->modifiers() & Qt::ShiftModifier;
    const bool alt = e->modifiers() & Qt::AltModifier;
    const int key = e->key();

    // --- набір тексту
    if (m_textIndex >= 0 && m_textIndex < m_shapes.size()) {
        Shape &s = m_shapes[m_textIndex];
        m_caretOn = true;
        if (key == Qt::Key_Escape) {
            commitText();
            return;
        }
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            if (shift)
                s.text += QLatin1Char('\n');
            else
                commitText();
            update();
            return;
        }
        if (key == Qt::Key_Backspace) {
            if (!s.text.isEmpty())
                s.text.chop(1);
            update();
            return;
        }
        if (ctrl && key == Qt::Key_V) {
            s.text += QGuiApplication::clipboard()->text();
            update();
            return;
        }
        const QString t = e->text();
        if (!ctrl && !t.isEmpty() && t.at(0).isPrint()) {
            s.text += t;
            update();
        }
        return;
    }

    if (key == Qt::Key_Escape) {
        if (hasSelectedShape()) {
            m_selectedShape = -1;
            update();
            return;
        }
        emit cancelled();
        return;
    }

    if ((key == Qt::Key_Delete || key == Qt::Key_Backspace) && hasSelectedShape()) {
        deleteSelectedShape();
        return;
    }

    if (ctrl) {
        switch (key) {
        case Qt::Key_C: finish(Action::Copy); return;
        case Qt::Key_S: finish(shift ? Action::SaveAs : Action::Save); return;
        case Qt::Key_P: finish(Action::Print); return;
        case Qt::Key_T: finish(Action::Pin); return;
        case Qt::Key_Z: shift ? redo() : undo(); return;
        case Qt::Key_Y: redo(); return;
        case Qt::Key_A: selectAll(); return;
        default: break;
        }
    }

    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        finish(Action::Copy);
        return;
    }

    if (m_hasSel) {
        const int step = shift ? 10 : 1;
        int dx = 0, dy = 0;
        switch (key) {
        case Qt::Key_Left:  dx = -step; break;
        case Qt::Key_Right: dx = step; break;
        case Qt::Key_Up:    dy = -step; break;
        case Qt::Key_Down:  dy = step; break;
        default: break;
        }
        if (dx || dy) {
            if (hasSelectedShape() && !alt) {
                pushUndo();
                Annotation::translate(m_shapes[m_selectedShape], QPointF(dx, dy));
                update();
            } else {
                alt ? resizeSelection(dx, dy) : moveSelection(dx, dy);
            }
            return;
        }

        if (!ctrl && !alt) {
            switch (key) {
            case Qt::Key_P: toggleTool(Tool::Pen); return;
            case Qt::Key_L: toggleTool(Tool::Line); return;
            case Qt::Key_A: toggleTool(Tool::Arrow); return;
            case Qt::Key_R: toggleTool(Tool::Rect); return;
            case Qt::Key_E: toggleTool(Tool::Ellipse); return;
            case Qt::Key_M: toggleTool(Tool::Marker); return;
            case Qt::Key_T: toggleTool(Tool::Text); return;
            case Qt::Key_B: toggleTool(Tool::Pixelate); return;
            case Qt::Key_N: toggleTool(Tool::Counter); return;
            case Qt::Key_Space:
            case Qt::Key_V: setTool(Tool::None); return;
            default: break;
            }
        }
    } else if (!ctrl && key == Qt::Key_C) {
        copyColorUnderCursor();
        return;
    }

    QWidget::keyPressEvent(e);
}

// ---------------------------------------------------------------------------
// Малювання

void CaptureOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.drawPixmap(0, 0, m_shot);

    const bool showSel = m_hasSel || m_drag == Drag::Selecting;
    QRect sel = showSel ? m_sel : m_hoverWindow;

    // Затемнення всього, крім виділення
    QRegion dim(rect());
    if (!sel.isEmpty())
        dim -= QRegion(sel);
    p.save();
    p.setClipRegion(dim);
    p.fillRect(rect(), QColor(0, 0, 0, 115));
    p.restore();

    // Малюнки користувача
    if (!m_shapes.isEmpty() && showSel && !sel.isEmpty()) {
        p.save();
        p.setClipRect(sel);
        for (const Shape &s : std::as_const(m_shapes))
            Annotation::draw(p, s, m_base, m_dpr);
        p.restore();
    }

    if (!sel.isEmpty()) {
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(QPen(kAccent, 1, showSel ? Qt::SolidLine : Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(sel.adjusted(-1, -1, 0, 0));
        if (m_hasSel && m_drag != Drag::Drawing)
            drawHandles(p, sel);
        drawSizeLabel(p, sel);
    }

    drawShapeSelection(p);
    drawTextCaret(p);

    if (!m_hasSel && m_mouseInside) {
        if (m_showMagnifier)
            drawMagnifier(p);
        if (m_drag == Drag::None)
            drawHelp(p);
    }
    if (m_showWidthHint)
        drawWidthHint(p);
}

void CaptureOverlay::drawHandles(QPainter &p, const QRect &r)
{
    const int hs = 7;
    const QPoint pts[] = {
        r.topLeft(), QPoint(r.center().x(), r.top()), r.topRight(),
        QPoint(r.right(), r.center().y()), r.bottomRight(),
        QPoint(r.center().x(), r.bottom()), r.bottomLeft(), QPoint(r.left(), r.center().y()),
    };
    p.setPen(QPen(kAccent, 1));
    p.setBrush(Qt::white);
    for (const QPoint &pt : pts)
        p.drawRect(QRect(pt.x() - hs / 2, pt.y() - hs / 2, hs - 1, hs - 1));
}

void CaptureOverlay::drawSizeLabel(QPainter &p, const QRect &r)
{
    const QString txt = QStringLiteral("%1 × %2").arg(qRound(r.width() * m_dpr)).arg(qRound(r.height() * m_dpr));
    QFont f = font();
    f.setPixelSize(12);
    p.setFont(f);
    const QFontMetrics fm(f);
    QRect tr(0, 0, fm.horizontalAdvance(txt) + 14, fm.height() + 6);
    tr.moveBottomLeft(QPoint(r.left(), r.top() - 6));
    if (tr.top() < 0)
        tr.moveTopLeft(QPoint(r.left() + 4, r.top() + 4));
    if (tr.right() > width() - 2)
        tr.moveRight(width() - 2);

    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 20, 22, 215));
    p.drawRoundedRect(tr, 4, 4);
    p.setPen(Qt::white);
    p.drawText(tr, Qt::AlignCenter, txt);
    p.restore();
}

void CaptureOverlay::drawMagnifier(QPainter &p)
{
    const int cells = 17;
    const int zoom = 8;
    const int box = cells * zoom;
    const int infoH = 40;

    const QPoint phys(int(std::floor(m_mouse.x() * m_dpr)), int(std::floor(m_mouse.y() * m_dpr)));
    const QImage src = m_base.copy(phys.x() - cells / 2, phys.y() - cells / 2, cells, cells);

    QRect boxR(m_mouse + QPoint(22, 22), QSize(box, box));
    if (boxR.right() > width() - 4)
        boxR.moveRight(m_mouse.x() - 22);
    if (boxR.bottom() + infoH > height() - 4)
        boxR.moveBottom(m_mouse.y() - 22 - infoH);

    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.drawImage(boxR, src);

    // перехрестя
    const int c0 = (cells / 2) * zoom;
    p.fillRect(QRect(boxR.left(), boxR.top() + c0, box, zoom), QColor(47, 140, 255, 60));
    p.fillRect(QRect(boxR.left() + c0, boxR.top(), zoom, box), QColor(47, 140, 255, 60));
    const QRect center(boxR.left() + c0, boxR.top() + c0, zoom - 1, zoom - 1);
    p.setPen(Qt::black);
    p.drawRect(center.adjusted(-1, -1, 1, 1));
    p.setPen(Qt::white);
    p.drawRect(center);

    // підпис: колір і координати
    const QColor col = m_base.valid(phys) ? m_base.pixelColor(phys) : QColor(Qt::black);
    const QRect info(boxR.left(), boxR.bottom() + 1, box, infoH);
    p.fillRect(info, QColor(20, 20, 22, 230));

    QFont f = font();
    f.setPixelSize(11);
    p.setFont(f);
    const QRect sw(info.left() + 6, info.top() + 5, 12, 12);
    p.fillRect(sw, col);
    p.setPen(QColor(255, 255, 255, 160));
    p.drawRect(sw.adjusted(0, 0, -1, -1));
    p.setPen(Qt::white);
    const QRect line1(sw.right() + 6, info.top() + 2, info.width() - (sw.right() - info.left()) - 12, 18);
    p.drawText(line1, Qt::AlignLeft | Qt::AlignVCenter, col.name().toUpper());
    p.drawText(line1, Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("%1, %2").arg(phys.x()).arg(phys.y()));
    f.setPixelSize(10);
    p.setFont(f);
    p.setPen(QColor(255, 255, 255, 170));
    p.drawText(QRect(info.left() + 6, info.top() + 20, info.width() - 12, 16), Qt::AlignLeft | Qt::AlignVCenter,
               tr2("C — копіювати колір", "C — copy color"));

    // рамка навколо всієї лупи (темна + світла, щоб було видно на будь-якому фоні)
    const QRect whole(boxR.topLeft(), QSize(box, box + infoH + 1));
    p.setPen(QColor(0, 0, 0, 160));
    p.setBrush(Qt::NoBrush);
    p.drawRect(whole.adjusted(-1, -1, 0, 0));
    p.setPen(QColor(255, 255, 255, 220));
    p.drawRect(whole.adjusted(0, 0, -1, -1));
    p.restore();
}

void CaptureOverlay::drawShapeSelection(QPainter &p)
{
    if (!hasSelectedShape() || m_selectedShape == m_textIndex || m_drag == Drag::Drawing)
        return;
    const QRectF r = Annotation::bounds(m_shapes[m_selectedShape]).adjusted(-4, -4, 4, 4);
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(0, 0, 0, 140), 1));
    p.drawRect(r.adjusted(-1, -1, 1, 1));
    p.setPen(QPen(Qt::white, 1, Qt::DashLine));
    p.drawRect(r);
    p.restore();
}

void CaptureOverlay::drawTextCaret(QPainter &p)
{
    if (m_textIndex < 0 || m_textIndex >= m_shapes.size())
        return;
    const Shape &s = m_shapes[m_textIndex];
    if (s.points.isEmpty())
        return;

    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    const QRectF tr = Annotation::textRect(s);
    p.setPen(QPen(QColor(255, 255, 255, 200), 1, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawRect(tr.adjusted(-4, -2, 4, 2));

    if (m_caretOn) {
        const QFontMetricsF fm(Annotation::textFont(s.width));
        const QStringList lines = s.text.split(QLatin1Char('\n'));
        const qreal x = s.points[0].x() + fm.horizontalAdvance(lines.last()) + 1;
        const qreal y = s.points[0].y() + (lines.size() - 1) * fm.lineSpacing();
        p.setPen(QPen(s.color, 2));
        p.drawLine(QPointF(x, y + 2), QPointF(x, y + fm.height() - 2));
    }
    p.restore();
}

void CaptureOverlay::drawWidthHint(QPainter &p)
{
    qreal d = m_width;
    if (m_tool == Tool::Marker)
        d = Annotation::markerWidth(m_width);
    else if (m_tool == Tool::Counter)
        d = Annotation::counterRadius(m_width) * 2;
    else if (m_tool == Tool::Text)
        d = Annotation::textFont(m_width).pixelSize();

    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    QColor c = m_color;
    c.setAlpha(170);
    p.setPen(QPen(Qt::white, 1));
    p.setBrush(c);
    p.drawEllipse(QPointF(m_mouse), qMax<qreal>(1.0, d / 2), qMax<qreal>(1.0, d / 2));

    const QString t = tr2("Товщина: %1", "Thickness: %1").arg(m_width);
    QFont f = font();
    f.setPixelSize(12);
    p.setFont(f);
    const QFontMetrics fm(f);
    QRect tr(0, 0, fm.horizontalAdvance(t) + 14, fm.height() + 6);
    tr.moveTopLeft(m_mouse + QPoint(int(d / 2) + 10, int(d / 2) + 10));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 20, 22, 215));
    p.drawRoundedRect(tr, 4, 4);
    p.setPen(Qt::white);
    p.drawText(tr, Qt::AlignCenter, t);
    p.restore();
}

void CaptureOverlay::drawHelp(QPainter &p)
{
    const QString t = tr2("Виділіть область мишею  •  Клік — вікно під курсором / весь екран  •  "
                                     "Ctrl+A — весь екран  •  Esc — вихід", "Drag to select an area  •  Click — window under cursor / full screen  •  Ctrl+A — full screen  •  Esc — exit");
    QFont f = font();
    f.setPixelSize(13);
    p.save();
    p.setFont(f);
    const QFontMetrics fm(f);
    QRect tr(0, 0, fm.horizontalAdvance(t) + 24, fm.height() + 12);
    tr.moveCenter(QPoint(width() / 2, 0));
    tr.moveBottom(height() - 24);
    if (tr.adjusted(-40, -40, 40, 40).contains(m_mouse) || tr.intersects(m_hoverWindow.adjusted(0, -30, 0, 0)))
        tr.moveTop(24);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 20, 22, 200));
    p.drawRoundedRect(tr, 6, 6);
    p.setPen(QColor(255, 255, 255, 230));
    p.drawText(tr, Qt::AlignCenter, t);
    p.restore();
}
