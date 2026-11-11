#pragma once

#include <QRect>
#include <QVector>

class QWidget;

namespace Platform {

// Прямокутники видимих вікон верхнього рівня у фізичних (native) координатах,
// від верхнього до нижнього за Z-порядком. На інших ОС — порожній список.
QVector<QRect> windowRects();

// Вивести вікно на передній план (Windows інколи цього не дозволяє без підказки).
void forceForeground(QWidget *w);

} // namespace Platform
