#include "icons.h"

#include <QApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPolygonF>

#include <cmath>
#include <initializer_list>

namespace Icons {

namespace {

// Усі гліфи намальовані в системі координат 24x24.
void drawGlyph(QPainter &p, Id id, const QColor &c, qreal pw)
{
    const QPen pen(c, pw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    auto line = [&p](qreal x1, qreal y1, qreal x2, qreal y2) { p.drawLine(QPointF(x1, y1), QPointF(x2, y2)); };
    auto poly = [&p](std::initializer_list<QPointF> pts) {
        QPolygonF pg;
        for (const QPointF &pt : pts)
            pg << pt;
        p.drawPolyline(pg);
    };
    auto closedPoly = [&p](std::initializer_list<QPointF> pts) {
        QPolygonF pg;
        for (const QPointF &pt : pts)
            pg << pt;
        p.drawPolygon(pg);
    };
    auto undoShape = [&]() {
        QPainterPath path;
        path.moveTo(8, 9);
        path.lineTo(14.5, 9);
        path.cubicTo(21, 9, 21, 19, 14.5, 19);
        path.lineTo(7, 19);
        p.drawPath(path);
        poly({{11.5, 5.5}, {8, 9}, {11.5, 12.5}});
    };

    switch (id) {
    case Pen:
        closedPoly({{4, 20}, {5, 15}, {16, 4}, {20, 8}, {9, 19}});
        line(14, 6, 18, 10);
        break;
    case Line:
        line(5, 19, 19, 5);
        break;
    case Arrow:
        line(5, 19, 18.5, 5.5);
        poly({{10, 5}, {19, 5}, {19, 14}});
        break;
    case Rect:
        p.drawRoundedRect(QRectF(4, 6, 16, 12), 1.5, 1.5);
        break;
    case Ellipse:
        p.drawEllipse(QRectF(3.5, 5.5, 17, 13));
        break;
    case Marker: {
        QColor hc = c;
        hc.setAlphaF(0.45);
        p.setPen(QPen(hc, 5, Qt::SolidLine, Qt::FlatCap));
        line(3, 19, 21, 19);
        p.setPen(pen);
        closedPoly({{8, 14}, {14, 3.5}, {18.5, 6}, {12.5, 16.5}});
        line(8, 14, 7.5, 17);
        line(7.5, 17, 12.5, 16.5);
        break;
    }
    case Text:
        line(5, 5, 19, 5);
        line(12, 5, 12, 20);
        line(9, 20, 15, 20);
        break;
    case Pixelate: {
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                QColor cc = c;
                cc.setAlphaF(((i + j) % 2) ? 0.35 : 0.95);
                p.setBrush(cc);
                p.drawRoundedRect(QRectF(3.5 + i * 6, 3.5 + j * 6, 5, 5), 1, 1);
            }
        }
        break;
    }
    case Counter:
        p.drawEllipse(QRectF(3.5, 3.5, 17, 17));
        line(10, 9, 12.5, 7);
        line(12.5, 7, 12.5, 17);
        break;
    case Undo:
        undoShape();
        break;
    case Redo:
        p.save();
        p.translate(24, 0);
        p.scale(-1, 1);
        undoShape();
        p.restore();
        break;
    case Copy:
        p.drawRoundedRect(QRectF(8, 8, 12, 12), 2, 2);
        poly({{8, 16}, {5.5, 16}, {4, 14.5}, {4, 5.5}, {5.5, 4}, {14.5, 4}, {16, 5.5}, {16, 8}});
        break;
    case Save:
        p.drawRoundedRect(QRectF(4, 4, 16, 16), 2, 2);
        p.drawRect(QRectF(8, 4, 7, 5));
        p.drawRect(QRectF(7, 13, 10, 7));
        break;
    case SaveAs:
        p.drawRoundedRect(QRectF(3, 3, 14, 14), 2, 2);
        p.drawRect(QRectF(6.5, 3, 6, 4));
        line(15, 21, 21, 15);
        line(13, 21, 15, 21);
        line(13, 21, 13, 19);
        line(13, 19, 19, 13);
        line(19, 13, 21, 15);
        break;
    case Folder:
        closedPoly({{3, 6}, {9, 6}, {11, 8}, {21, 8}, {21, 19}, {3, 19}});
        break;
    case Pin:
        p.drawRoundedRect(QRectF(8, 3, 8, 3), 1, 1);
        closedPoly({{9.5, 6}, {14.5, 6}, {16.5, 13}, {7.5, 13}});
        line(12, 13, 12, 21);
        break;
    case Print:
        p.drawRect(QRectF(7, 3, 10, 5));
        p.drawRoundedRect(QRectF(3, 8, 18, 9), 2, 2);
        p.drawRect(QRectF(7, 14, 10, 7));
        break;
    case Close:
        line(6, 6, 18, 18);
        line(18, 6, 6, 18);
        break;
    case Gallery:
        p.drawRoundedRect(QRectF(4, 4, 7, 7), 1.5, 1.5);
        p.drawRoundedRect(QRectF(13, 4, 7, 7), 1.5, 1.5);
        p.drawRoundedRect(QRectF(4, 13, 7, 7), 1.5, 1.5);
        p.drawRoundedRect(QRectF(13, 13, 7, 7), 1.5, 1.5);
        break;
    case Gear: {
        p.save();
        p.translate(12, 12);
        p.setPen(QPen(c, pw * 1.7, Qt::SolidLine, Qt::FlatCap));
        for (int i = 0; i < 8; ++i) {
            p.drawLine(QPointF(0, -6.5), QPointF(0, -9.5));
            p.rotate(45);
        }
        p.setPen(pen);
        p.drawEllipse(QPointF(0, 0), 6.5, 6.5);
        p.drawEllipse(QPointF(0, 0), 2.5, 2.5);
        p.restore();
        break;
    }
    case Capture:
        poly({{4, 9}, {4, 4}, {9, 4}});
        poly({{15, 4}, {20, 4}, {20, 9}});
        poly({{20, 15}, {20, 20}, {15, 20}});
        poly({{9, 20}, {4, 20}, {4, 15}});
        line(12, 9, 12, 15);
        line(9, 12, 15, 12);
        break;
    case Fullscreen:
        p.drawRoundedRect(QRectF(3, 4, 18, 12), 1.5, 1.5);
        line(12, 16, 12, 20);
        line(8, 20, 16, 20);
        break;
    case Trash:
        line(4, 6, 20, 6);
        poly({{9, 6}, {9, 3.5}, {15, 3.5}, {15, 6}});
        poly({{6, 6}, {7, 20.5}, {17, 20.5}, {18, 6}});
        line(10, 10, 10, 17);
        line(14, 10, 14, 17);
        break;
    case Refresh:
        p.drawArc(QRectF(5, 5, 14, 14), 45 * 16, 300 * 16);
        poly({{17.5, 2.5}, {17, 7}, {12.5, 6.5}});
        break;
    case Timer:
        p.drawEllipse(QRectF(4.5, 5.5, 15, 15));
        line(12, 13, 12, 9);
        line(12, 13, 15, 15);
        line(10, 2.5, 14, 2.5);
        break;
    case Info:
        p.drawEllipse(QRectF(3.5, 3.5, 17, 17));
        line(12, 11, 12, 16.5);
        p.drawPoint(QPointF(12, 7.8));
        break;
    case Exit:
        p.drawArc(QRectF(5, 5.5, 14, 14), 120 * 16, 300 * 16);
        line(12, 3, 12, 11);
        break;
    case Open:
        poly({{11, 5}, {5, 5}, {5, 19}, {19, 19}, {19, 13}});
        line(12, 12, 20, 4);
        poly({{14, 4}, {20, 4}, {20, 10}});
        break;
    case Rename:
        line(4, 20, 20, 20);
        closedPoly({{6, 17}, {6.5, 13.5}, {15, 5}, {18, 8}, {9.5, 16.5}});
        break;
    case Cursor:
        closedPoly({{6, 3}, {6, 19}, {10.2, 15.2}, {13.2, 21.5}, {15.8, 20.3}, {12.8, 14}, {18.5, 14}});
        break;
    case Blur: {
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 4; ++i) {
            QColor cc = c;
            cc.setAlphaF(0.25 + i * 0.2);
            p.setBrush(cc);
            p.drawEllipse(QPointF(12, 12), 9 - i * 2.2, 9 - i * 2.2);
        }
        break;
    }
    }
}

} // namespace

QIcon icon(Id id, const QColor &color)
{
    QIcon ic;
    for (int s : {16, 20, 24, 32, 40, 48}) {
        QPixmap pm(s, s);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.scale(s / 24.0, s / 24.0);
        drawGlyph(p, id, color, 1.8);
        p.end();
        ic.addPixmap(pm);
    }
    return ic;
}

QIcon themed(Id id)
{
    return icon(id, QApplication::palette().color(QPalette::WindowText));
}

QIcon colorSwatch(const QColor &c, int size)
{
    QIcon ic;
    for (int s : {size, size * 2}) {
        QPixmap pm(s, s);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal k = s / 24.0;
        p.setPen(QPen(QColor(255, 255, 255, 230), 2 * k));
        p.setBrush(c);
        p.drawEllipse(QRectF(3 * k, 3 * k, 18 * k, 18 * k));
        p.end();
        ic.addPixmap(pm);
    }
    return ic;
}

// Лого Lumio: діафрагма об'єктива (світло, що проходить крізь лінзу)
// на теплому градієнті та маленький спалах.
void drawLogo(QPainter &p, qreal size)
{
    p.save();
    p.scale(size / 24.0, size / 24.0);

    // фон: заокруглений квадрат з градієнтом «світанку»
    QLinearGradient g(2, 1, 22, 23);
    g.setColorAt(0.0, QColor(0xff, 0xc1, 0x4d));
    g.setColorAt(0.45, QColor(0xff, 0x5f, 0x8f));
    g.setColorAt(1.0, QColor(0x6c, 0x4b, 0xff));
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawRoundedRect(QRectF(1, 1, 22, 22), 6, 6);

    // м'який відблиск зверху
    QLinearGradient gl(0, 1, 0, 12);
    gl.setColorAt(0, QColor(255, 255, 255, 70));
    gl.setColorAt(1, QColor(255, 255, 255, 0));
    p.setBrush(gl);
    p.drawRoundedRect(QRectF(1, 1, 22, 11), 6, 6);

    // діафрагма: коло + 6 пелюсток
    const QPointF c(12, 12.4);
    const qreal R = 7.0, r = 2.9;
    const qreal pw = size <= 20 ? 1.9 : 1.45;
    p.setPen(QPen(Qt::white, pw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(QColor(255, 255, 255, 38));
    p.drawEllipse(c, R, R);
    p.setBrush(Qt::NoBrush);
    const qreal kPi = 3.14159265358979323846;
    for (int i = 0; i < 6; ++i) {
        const qreal a0 = (i * 60.0 - 90.0) * kPi / 180.0;
        const qreal a1 = ((i + 1) * 60.0 - 90.0) * kPi / 180.0;
        const QPointF v0 = c + QPointF(std::cos(a0) * r, std::sin(a0) * r);
        const QPointF v1 = c + QPointF(std::cos(a1) * r, std::sin(a1) * r);
        QPointF d = v1 - v0;
        const qreal len = std::hypot(d.x(), d.y());
        d /= len;
        // продовжити сторону шестикутника до перетину з колом
        const QPointF m = v0 - c;
        const qreal b = m.x() * d.x() + m.y() * d.y();
        const qreal cc = m.x() * m.x() + m.y() * m.y() - R * R;
        const qreal t = -b + std::sqrt(qMax<qreal>(0, b * b - cc));
        p.drawLine(v0, v0 + d * t);
    }

    // спалах (4-променева зірочка) у верхньому правому куті
    if (size >= 20) {
        const QPointF s(18.6, 5.4);
        const qreal L = 2.6, w = 0.75;
        QPainterPath star;
        star.moveTo(s + QPointF(0, -L));
        star.quadTo(s + QPointF(w * 0.3, -w * 0.3), s + QPointF(L, 0));
        star.quadTo(s + QPointF(w * 0.3, w * 0.3), s + QPointF(0, L));
        star.quadTo(s + QPointF(-w * 0.3, w * 0.3), s + QPointF(-L, 0));
        star.quadTo(s + QPointF(-w * 0.3, -w * 0.3), s + QPointF(0, -L));
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawPath(star);
    }
    p.restore();
}

QPixmap appPixmap(int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    drawLogo(p, size);
    p.end();
    return pm;
}

QIcon appIcon()
{
    QIcon ic;
    for (int s : {16, 20, 24, 32, 40, 48, 64, 128, 256})
        ic.addPixmap(appPixmap(s));
    return ic;
}

} // namespace Icons
