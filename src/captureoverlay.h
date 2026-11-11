#pragma once

#include "annotation.h"

#include <QImage>
#include <QMap>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

class QFrame;
class QToolButton;
class QScreen;

// Повноекранне вікно поверх одного монітора: показує заморожений знімок,
// дає виділити область, намалювати поверх неї та виконати дію (копіювати/зберегти/...).
class CaptureOverlay : public QWidget
{
    Q_OBJECT
public:
    enum class Action { Copy, Save, SaveAs, Pin, Print };

    // shot — знімок екрана з правильним devicePixelRatio;
    // windows — прямокутники вікон у локальних логічних координатах цього екрана
    CaptureOverlay(QScreen *screen, const QPixmap &shot, const QVector<QRect> &windows, QWidget *parent = nullptr);

    void showOnScreen();
    void activate();
    QScreen *targetScreen() const { return m_screen; }
    QImage renderResult() const;

signals:
    void actionRequested(CaptureOverlay::Action action, const QImage &image, const QRect &globalRect);
    void cancelled();

protected:
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void leaveEvent(QEvent *e) override;

private:
    enum class Drag { None, Selecting, Moving, Resizing, Drawing, MovingShape };

    void buildPanels();
    QToolButton *addButton(QFrame *panel, int icon, const QString &tip);
    void addSeparator(QFrame *panel, bool vertical);
    void updatePanels();
    void updateUndoButtons();
    void setTool(Tool t);
    void toggleTool(Tool t);
    void finish(Action a);
    void undo();
    void redo();
    void commitText();
    void editText(int index);
    void pushUndo();
    int shapeAt(const QPoint &pos) const;
    bool hasSelectedShape() const { return m_selectedShape >= 0 && m_selectedShape < m_shapes.size(); }
    void deleteSelectedShape();
    void setObscureStyle(int style);
    void drawShapeSelection(QPainter &p);
    void startShape(const QPoint &pos);
    int nextCounterNumber() const;
    void chooseColor();
    void setColor(const QColor &c);
    void setPenWidth(int w);
    void updateCursor(const QPoint &pos);
    void updateHover();
    Qt::Edges edgesAt(const QPoint &pos) const;
    void selectAll();
    void resetSelection();
    void moveSelection(int dx, int dy);
    void resizeSelection(int dx, int dy);
    void copyColorUnderCursor();
    QPointF constrained(const QPointF &from, const QPointF &to, Tool t) const;

    void drawMagnifier(QPainter &p);
    void drawSizeLabel(QPainter &p, const QRect &r);
    void drawHandles(QPainter &p, const QRect &r);
    void drawTextCaret(QPainter &p);
    void drawWidthHint(QPainter &p);
    void drawHelp(QPainter &p);

    QScreen *m_screen;
    QPixmap m_shot;
    QImage m_base;   // той самий знімок у фізичних пікселях
    qreal m_dpr = 1.0;

    QVector<QRect> m_windows;
    QRect m_hoverWindow;

    QRect m_sel;
    bool m_hasSel = false;
    Drag m_drag = Drag::None;
    Qt::Edges m_edges;
    QPoint m_pressPos;
    QRect m_selAtPress;
    QPoint m_mouse;
    bool m_mouseInside = true;

    Tool m_tool = Tool::None;
    QColor m_color;
    int m_width = 3;
    QVector<Shape> m_shapes;
    QVector<QVector<Shape>> m_undoStack;
    QVector<QVector<Shape>> m_redoStack;
    int m_selectedShape = -1;
    Shape m_shapeAtPress;
    bool m_shapeMoved = false;
    int m_obscureStyle = ObscurePixelate;
    bool m_textIsNew = false;
    QString m_textOriginal;
    int m_textIndex = -1;
    bool m_caretOn = true;
    QTimer m_caretTimer;
    QTimer m_hintTimer;
    bool m_showWidthHint = false;
    bool m_showMagnifier = true;

    QFrame *m_toolPanel = nullptr;
    QFrame *m_actionPanel = nullptr;
    QMap<Tool, QToolButton *> m_toolButtons;
    QToolButton *m_colorButton = nullptr;
    QToolButton *m_obscureButton = nullptr;
    QToolButton *m_undoButton = nullptr;
    QToolButton *m_redoButton = nullptr;
};
