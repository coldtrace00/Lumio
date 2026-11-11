#include "annotation.h"

#include <QFontMetricsF>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPolygonF>

#include <cmath>

namespace Annotation {

namespace {

QRectF normRect(const Shape &s)
{
    if (s.points.size() < 2)
        return QRectF();
    return QRectF(s.points[0], s.points[1]).normalized();
}

void drawArrow(QPainter &p, const QPointF &a, const QPointF &b, const QColor &color, int width)
{
    const QLineF l(a, b);
    const qreal len = l.length();
    if (len < 1.0)
        return;
    const QPointF u((b.x() - a.x()) / len, (b.y() - a.y()) / len);
    const QPointF n(-u.y(), u.x());
    const qreal head = qMin<qreal>(10.0 + width * 3.0, len * 0.9);
    const QPointF base = b - u * head;

    p.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawLine(a, b - u * (head * 0.6));

    QPolygonF tri;
    tri << b << (base + n * head * 0.45) << (base - n * head * 0.45);
    p.setPen(QPen(color, 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(color);
    p.drawPolygon(tri);
}

QImage blurred(const QImage &src, int radius)
{
    // Швидке «розмиття»: зменшити і плавно збільшити, двічі
    QImage img = src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int k = qMax(2, radius);
    for (int pass = 0; pass < 2; ++pass) {
        const QImage small = img.scaled(qMax(1, img.width() / k), qMax(1, img.height() / k),
                                        Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        img = small.scaled(src.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    return img;
}

void drawObscure(QPainter &p, const Shape &s, const QImage &base, qreal dpr)
{
    const QRectF r = normRect(s);
    if (r.width() < 1 || r.height() < 1)
        return;

    if (s.style == ObscureSolid) {
        p.setPen(Qt::NoPen);
        p.setBrush(s.color);
        p.drawRoundedRect(r, 2, 2);
        return;
    }
    if (base.isNull())
        return;

    const QRect phys = QRectF(r.x() * dpr, r.y() * dpr, r.width() * dpr, r.height() * dpr)
                           .toAlignedRect()
                           .intersected(base.rect());
    if (phys.isEmpty())
        return;
    const QRectF target(phys.x() / dpr, phys.y() / dpr, phys.width() / dpr, phys.height() / dpr);

    if (s.style == ObscurePixelate) {
        const int blk = qMax(2, int((4 + s.width * 2) * dpr));
        const QImage part = base.copy(phys);
        const QImage small = part.scaled(qMax(1, part.width() / blk), qMax(1, part.height() / blk),
                                         Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        p.drawImage(target, small.scaled(part.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation));
        return;
    }

    // Розмиття: беремо трохи більшу ділянку, щоб краї не «світились»
    const int radius = qMax(3, int((3 + s.width * 2) * dpr));
    const QRect grown = phys.adjusted(-radius * 2, -radius * 2, radius * 2, radius * 2).intersected(base.rect());
    const QImage b = blurred(base.copy(grown), radius);
    p.drawImage(target, b.copy(phys.translated(-grown.topLeft())));
    if (s.style == ObscureTintedBlur) {
        QColor c = s.color;
        c.setAlpha(qMin(c.alpha(), 105));
        p.fillRect(target, c);
    }
}

} // namespace

QFont textFont(int width)
{
    QFont f(QStringLiteral("Segoe UI"));
    f.setStyleHint(QFont::SansSerif);
    f.setPixelSize(10 + width * 3);
    return f;
}

QRectF textRect(const Shape &s)
{
    if (s.points.isEmpty())
        return QRectF();
    const QFontMetricsF fm(textFont(s.width));
    const QString t = s.text.isEmpty() ? QStringLiteral(" ") : s.text;
    QRectF r = fm.boundingRect(QRectF(s.points[0], QSizeF(100000, 100000)), Qt::AlignLeft | Qt::AlignTop, t);
    if (s.text.endsWith(QLatin1Char('\n')))
        r.setHeight(r.height() + fm.lineSpacing());
    return r;
}

qreal counterRadius(int width)
{
    return 9.0 + width * 1.5;
}

qreal markerWidth(int width)
{
    return width * 4.0 + 6.0;
}

QRectF bounds(const Shape &s)
{
    if (s.points.isEmpty())
        return QRectF();
    switch (s.tool) {
    case Tool::Text:
        return textRect(s);
    case Tool::Counter: {
        const qreal r = counterRadius(s.width);
        return QRectF(s.points[0] - QPointF(r, r), QSizeF(2 * r, 2 * r));
    }
    default: {
        QRectF r(s.points[0], QSizeF(0, 0));
        for (const QPointF &pt : s.points)
            r = r.united(QRectF(pt, QSizeF(0, 0)));
        const qreal w = (s.tool == Tool::Marker ? markerWidth(s.width) : s.width) / 2.0
                        + (s.tool == Tool::Arrow ? 6 + s.width * 1.5 : 0);
        return r.normalized().adjusted(-w, -w, w, w);
    }
    }
}

bool hitTest(const Shape &s, const QPointF &pt)
{
    if (s.points.isEmpty())
        return false;
    switch (s.tool) {
    case Tool::Text:
        return textRect(s).adjusted(-4, -4, 4, 4).contains(pt);
    case Tool::Counter:
        return QLineF(s.points[0], pt).length() <= counterRadius(s.width) + 3;
    case Tool::Pixelate:
        return normRect(s).adjusted(-3, -3, 3, 3).contains(pt);
    default: {
        QPainterPath path(s.points[0]);
        if (s.tool == Tool::Rect) {
            path = QPainterPath();
            path.addRect(normRect(s));
        } else if (s.tool == Tool::Ellipse) {
            path = QPainterPath();
            path.addEllipse(normRect(s));
        } else {
            for (int i = 1; i < s.points.size(); ++i)
                path.lineTo(s.points[i]);
        }
        QPainterPathStroker stroker;
        const qreal w = s.tool == Tool::Marker ? markerWidth(s.width) : s.width;
        stroker.setWidth(qMax<qreal>(w + 8, 12));
        stroker.setCapStyle(Qt::RoundCap);
        return stroker.createStroke(path).contains(pt);
    }
    }
}

void translate(Shape &s, const QPointF &delta)
{
    for (QPointF &pt : s.points)
        pt += delta;
}

void draw(QPainter &p, const Shape &s, const QImage &base, qreal dpr)
{
    if (s.points.isEmpty())
        return;

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    const QPen pen(s.color, s.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    switch (s.tool) {
    case Tool::Pen:
    case Tool::Marker: {
        QColor c = s.color;
        qreal w = s.width;
        if (s.tool == Tool::Marker) {
            c.setAlpha(qMin(c.alpha(), 110));
            w = markerWidth(s.width);
        }
        // Шлях малюється за один раз — напівпрозорий маркер не темнішає на перетинах
        QPainterPath path(s.points[0]);
        if (s.points.size() == 1)
            path.lineTo(s.points[0] + QPointF(0.01, 0));
        for (int i = 1; i < s.points.size(); ++i)
            path.lineTo(s.points[i]);
        p.setPen(QPen(c, w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        break;
    }
    case Tool::Line:
        if (s.points.size() >= 2) {
            p.setPen(pen);
            p.drawLine(s.points[0], s.points[1]);
        }
        break;
    case Tool::Arrow:
        if (s.points.size() >= 2)
            drawArrow(p, s.points[0], s.points[1], s.color, s.width);
        break;
    case Tool::Rect:
        p.setPen(QPen(s.color, s.width, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        p.setBrush(Qt::NoBrush);
        p.drawRect(normRect(s));
        break;
    case Tool::Ellipse:
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(normRect(s));
        break;
    case Tool::Text:
        if (!s.text.isEmpty()) {
            p.setPen(s.color);
            p.setFont(textFont(s.width));
            p.drawText(textRect(s), Qt::AlignLeft | Qt::AlignTop, s.text);
        }
        break;
    case Tool::Pixelate:
        drawObscure(p, s, base, dpr);
        break;
    case Tool::Counter: {
        const QPointF c = s.points[0];
        const qreal r = counterRadius(s.width);
        p.setPen(QPen(Qt::white, qMax<qreal>(1.5, r * 0.12)));
        p.setBrush(s.color);
        p.drawEllipse(c, r, r);
        QFont f(QStringLiteral("Segoe UI"));
        f.setBold(true);
        f.setPixelSize(qMax(8, int(r * 1.05)));
        p.setFont(f);
        const bool light = (0.299 * s.color.red() + 0.587 * s.color.green() + 0.114 * s.color.blue()) > 170;
        p.setPen(light ? Qt::black : Qt::white);
        p.drawText(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r), Qt::AlignCenter, QString::number(s.number));
        break;
    }
    case Tool::None:
        break;
    }
    p.restore();
}

} // namespace Annotation
