#pragma once

#include <QImage>
#include <QWidget>

// Знімок, «приклеєний» поверх усіх вікон. Можна тягати, масштабувати колесом,
// змінювати прозорість (Ctrl+колесо). Подвійний клік або Esc — закрити.
class PinWindow : public QWidget
{
    Q_OBJECT
public:
    explicit PinWindow(const QImage &image, const QPoint &globalPos = QPoint(), QWidget *parent = nullptr);

signals:
    void savedToGallery(const QString &path);

protected:
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void contextMenuEvent(QContextMenuEvent *e) override;

private:
    void setZoom(qreal zoom, const QPoint &anchorGlobal);
    void saveToGallery();

    QImage m_image;
    QSizeF m_baseSize;
    qreal m_zoom = 1.0;
    bool m_dragging = false;
    QPoint m_dragOffset;
};
