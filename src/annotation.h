#pragma once

#include <QColor>
#include <QFont>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

class QPainter;

enum class Tool { None, Pen, Line, Arrow, Rect, Ellipse, Marker, Text, Pixelate, Counter };

// Як приховувати інформацію інструментом «Розмиття»
enum ObscureStyle { ObscurePixelate = 0, ObscureBlur = 1, ObscureTintedBlur = 2, ObscureSolid = 3 };

// Один елемент малюнка поверх скріншота.
// Координати — логічні пікселі екрана, на якому зроблено знімок.
struct Shape {
    Tool tool = Tool::None;
    QColor color;
    int width = 3;
    QVector<QPointF> points; // Pen/Marker: ламана; Line/Arrow/Rect/Ellipse/Pixelate: 2 точки; Text/Counter: 1 точка
    QString text;
    int number = 0;
    int style = ObscurePixelate; // для Tool::Pixelate
};

namespace Annotation {

QFont textFont(int width);
QRectF textRect(const Shape &s);
qreal counterRadius(int width);
qreal markerWidth(int width);

QRectF bounds(const Shape &s);                       // приблизні межі фігури
bool hitTest(const Shape &s, const QPointF &pt);      // чи влучили курсором у фігуру
void translate(Shape &s, const QPointF &delta);

// base — повний знімок екрана у фізичних пікселях, dpr — масштаб екрана
void draw(QPainter &p, const Shape &s, const QImage &base, qreal dpr);

} // namespace Annotation
