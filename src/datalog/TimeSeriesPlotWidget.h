#pragma once
#include <QWidget>
#include <QVector>
#include <QColor>

namespace datalog {

class LogTable;

class TimeSeriesPlotWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimeSeriesPlotWidget(QWidget *parent = nullptr);
    void setTable(const LogTable *t);
    void setVisibleColumns(const QVector<int> &cols);   // column indices to plot, top-to-bottom
    void setXRangeMs(double t0, double t1);             // <0,0> => auto fit
    void resetView();
    QVector<int> visibleColumns() const { return m_visibleCols; }

signals:
    void crosshairMoved(double timeMs);

protected:
    void paintEvent(QPaintEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    const LogTable *m_t = nullptr;
    QVector<int>    m_visibleCols;
    double m_t0 = 0, m_t1 = 0;
    bool   m_panning = false;
    int    m_panAnchorX = 0;
    double m_panT0 = 0, m_panT1 = 0;
    double m_crosshairT = -1;
    double m_pxToTime(int x, const QRect &plotArea) const;
    int    m_timeToPx(double t, const QRect &plotArea) const;
    void   drawTrack(QPainter &p, const QRect &row, int colIdx, const QColor &color);
};

} // namespace datalog
