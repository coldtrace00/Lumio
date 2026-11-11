#include "pinwindow.h"
#include "i18n.h"

#include "compat.h"
#include "icons.h"
#include "imageio.h"

#include <QContextMenuEvent>
#include <QCursor>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QScreen>

namespace {
const int kBorder = 1;
}

PinWindow::PinWindow(const QImage &image, const QPoint &globalPos, QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
    , m_image(image)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr2("Закріплений знімок", "Pinned screenshot"));
    setCursor(Qt::OpenHandCursor);
    setToolTip(tr2("Перетягуйте мишею\nКолесо — масштаб, Ctrl+колесо — прозорість\n"
                              "Подвійний клік або Esc — закрити\nПравий клік — меню", "Drag with the mouse\nWheel — zoom, Ctrl+wheel — opacity\nDouble-click or Esc — close\nRight-click — menu"));

    const QPoint anchor = globalPos.isNull() ? QCursor::pos() : globalPos;
    qreal dpr = m_image.devicePixelRatio();
    if (qFuzzyCompare(dpr, 1.0)) {
        // Файл з диска: показуємо 1:1 у фізичних пікселях екрана
        if (QScreen *s = QGuiApplication::screenAt(anchor))
            dpr = s->devicePixelRatio();
    }
    m_baseSize = QSizeF(m_image.size()) / dpr;

    // Не більше 90% екрана
    if (QScreen *s = QGuiApplication::screenAt(anchor)) {
        const QSize avail = s->availableGeometry().size() * 0.9;
        if (m_baseSize.width() > avail.width() || m_baseSize.height() > avail.height())
            m_zoom = qMin(avail.width() / m_baseSize.width(), avail.height() / m_baseSize.height());
    }

    resize((m_baseSize * m_zoom).toSize() + QSize(2 * kBorder, 2 * kBorder));
    if (globalPos.isNull())
        move(anchor - rect().center());
    else
        move(globalPos - QPoint(kBorder, kBorder));
}

void PinWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, !qFuzzyCompare(m_zoom, 1.0));
    p.drawImage(QRectF(kBorder, kBorder, width() - 2 * kBorder, height() - 2 * kBorder), m_image);
    p.setPen(QPen(QColor(0x2f, 0x8c, 0xff), kBorder));
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

void PinWindow::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = Compat::globalPos(e) - frameGeometry().topLeft();
        setCursor(Qt::ClosedHandCursor);
    }
}

void PinWindow::mouseMoveEvent(QMouseEvent *e)
{
    if (m_dragging)
        move(Compat::globalPos(e) - m_dragOffset);
}

void PinWindow::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);
    }
}

void PinWindow::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        close();
}

void PinWindow::setZoom(qreal zoom, const QPoint &anchorGlobal)
{
    zoom = qBound(0.1, zoom, 8.0);
    const QPointF local = anchorGlobal - frameGeometry().topLeft();
    const QPointF rel(local.x() / qMax(1, width()), local.y() / qMax(1, height()));
    m_zoom = zoom;
    const QSize newSize = (m_baseSize * m_zoom).toSize() + QSize(2 * kBorder, 2 * kBorder);
    const QPoint topLeft(qRound(anchorGlobal.x() - rel.x() * newSize.width()),
                         qRound(anchorGlobal.y() - rel.y() * newSize.height()));
    setGeometry(QRect(topLeft, newSize));
    update();
}

void PinWindow::wheelEvent(QWheelEvent *e)
{
    const int delta = e->angleDelta().y();
    if (delta == 0)
        return;
    if (e->modifiers() & Qt::ControlModifier) {
        setWindowOpacity(qBound(0.2, windowOpacity() + (delta > 0 ? 0.1 : -0.1), 1.0));
        return;
    }
    setZoom(delta > 0 ? m_zoom * 1.1 : m_zoom / 1.1, Compat::globalPos(e));
}

void PinWindow::keyPressEvent(QKeyEvent *e)
{
    const bool ctrl = e->modifiers() & Qt::ControlModifier;
    if (e->key() == Qt::Key_Escape) {
        close();
    } else if (ctrl && e->key() == Qt::Key_C) {
        ImageIO::copyToClipboard(m_image);
    } else if (ctrl && e->key() == Qt::Key_S) {
        saveToGallery();
    } else if (e->key() == Qt::Key_0) {
        setZoom(1.0, frameGeometry().center());
    } else {
        QWidget::keyPressEvent(e);
    }
}

void PinWindow::saveToGallery()
{
    QString err;
    const QString path = ImageIO::saveToGallery(m_image, &err);
    if (path.isEmpty())
        QMessageBox::warning(this, tr2("Помилка", "Error"), tr2("Не вдалося зберегти:\n%1", "Could not save:\n%1").arg(err));
    else
        emit savedToGallery(path);
}

void PinWindow::contextMenuEvent(QContextMenuEvent *e)
{
    QMenu menu(this);
    menu.addAction(Icons::themed(Icons::Copy), tr2("Копіювати\tCtrl+C", "Copy\tCtrl+C"), this,
                   [this]() { ImageIO::copyToClipboard(m_image); });
    menu.addAction(Icons::themed(Icons::Save), tr2("Зберегти в галерею\tCtrl+S", "Save to gallery\tCtrl+S"), this,
                   [this]() { saveToGallery(); });
    menu.addAction(Icons::themed(Icons::SaveAs), tr2("Зберегти як…", "Save as…"), this,
                   [this]() { ImageIO::saveAsDialog(this, m_image); });
    menu.addSeparator();
    menu.addAction(tr2("Масштаб 100%\t0", "Zoom 100%\t0"), this, [this]() { setZoom(1.0, frameGeometry().center()); });
    QMenu *op = menu.addMenu(tr2("Прозорість", "Opacity"));
    for (int v : {100, 80, 60, 40}) {
        QAction *a = op->addAction(QStringLiteral("%1%").arg(v), this, [this, v]() { setWindowOpacity(v / 100.0); });
        a->setCheckable(true);
        a->setChecked(qAbs(windowOpacity() * 100 - v) < 5);
    }
    menu.addSeparator();
    menu.addAction(Icons::themed(Icons::Close), tr2("Закрити\tEsc", "Close\tEsc"), this, &QWidget::close);
    menu.exec(e->globalPos());
}
