#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>

// Усі іконки малюються кодом (векторно), тому проєкту не потрібні файли-картинки.
namespace Icons {

enum Id {
    Pen, Line, Arrow, Rect, Ellipse, Marker, Text, Pixelate, Counter,
    Undo, Redo, Copy, Save, SaveAs, Pin, Print, Close,
    Gallery, Gear, Capture, Fullscreen, Trash, Folder, Refresh, Timer, Info, Exit, Open, Rename, Cursor, Blur
};

QIcon icon(Id id, const QColor &color = QColor(Qt::white));
QIcon themed(Id id);                       // колір тексту поточної теми
QIcon colorSwatch(const QColor &c, int size = 24);
QPixmap appPixmap(int size);
QIcon appIcon();

} // namespace Icons
