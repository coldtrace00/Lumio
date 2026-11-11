#pragma once
// Невеликі обгортки, щоб код однаково збирався з Qt 5.15 і Qt 6.x

#include <QMouseEvent>
#include <QWheelEvent>
#include <QtGlobal>

namespace Compat {

inline QPoint pos(const QMouseEvent *e)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position().toPoint();
#else
    return e->pos();
#endif
}

inline QPoint globalPos(const QMouseEvent *e)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->globalPosition().toPoint();
#else
    return e->globalPos();
#endif
}

inline QPoint pos(const QWheelEvent *e)
{
    return e->position().toPoint();
}

inline QPoint globalPos(const QWheelEvent *e)
{
    return e->globalPosition().toPoint();
}

} // namespace Compat
