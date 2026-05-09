#include "TimeSeriesPlotWidget.h"
#include "LogTable.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QToolTip>

#include <algorithm>
#include <limits>

namespace datalog {

namespace {
constexpr int LEFT_MARGIN   = 64;
constexpr int RIGHT_MARGIN  = 12;
constexpr int TOP_MARGIN    = 8;
constexpr int BOTTOM_MARGIN = 24;
constexpr int ROW_GAP       = 6;

const QColor kRowColors[] = {
    QColor(70,160,230), QColor(220,90,90), QColor(120,200,120),
    QColor(230,180,60), QColor(180,120,230), QColor(110,200,200),
    QColor(220,110,180), QColor(150,150,150)
};

} // namespace

TimeSeriesPlotWidget::TimeSeriesPlotWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumHeight(200);
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
    setFocusPolicy(Qt::StrongFocus);
}

void TimeSeriesPlotWidget::setTable(const LogTable *t)
{
    m_t = t;
    resetView();
    update();
}

void TimeSeriesPlotWidget::setVisibleColumns(const QVector<int> &cols)
{
    m_visibleCols = cols;
    update();
}

void TimeSeriesPlotWidget::setXRangeMs(double t0, double t1)
{
    if (t0 == 0 && t1 == 0) { resetView(); return; }
    m_t0 = t0; m_t1 = t1;
    update();
}

void TimeSeriesPlotWidget::resetView()
{
    if (!m_t || m_t->rowCount() == 0) { m_t0 = m_t1 = 0; return; }
    m_t0 = m_t->timeMs.first();
    m_t1 = m_t->timeMs.last();
    if (m_t1 <= m_t0) m_t1 = m_t0 + 1.0;
}

double TimeSeriesPlotWidget::m_pxToTime(int x, const QRect &plotArea) const
{
    if (plotArea.width() <= 0) return m_t0;
    double frac = double(x - plotArea.left()) / double(plotArea.width());
    return m_t0 + frac * (m_t1 - m_t0);
}

int TimeSeriesPlotWidget::m_timeToPx(double tMs, const QRect &plotArea) const
{
    if (m_t1 <= m_t0) return plotArea.left();
    double frac = (tMs - m_t0) / (m_t1 - m_t0);
    return plotArea.left() + int(frac * plotArea.width());
}

void TimeSeriesPlotWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), palette().base());

    if (!m_t || m_t->rowCount() == 0 || m_visibleCols.isEmpty()) {
        p.setPen(palette().color(QPalette::Disabled, QPalette::WindowText));
        p.drawText(rect(), Qt::AlignCenter,
                   m_t ? tr("Select channels on the left") : tr("No log loaded"));
        return;
    }

    int n = m_visibleCols.size();
    int rowH = (height() - TOP_MARGIN - BOTTOM_MARGIN - ROW_GAP * (n - 1)) / n;
    if (rowH < 30) rowH = 30;

    QRect plotArea(LEFT_MARGIN, TOP_MARGIN, width() - LEFT_MARGIN - RIGHT_MARGIN,
                   height() - TOP_MARGIN - BOTTOM_MARGIN);

    int y = TOP_MARGIN;
    for (int i = 0; i < n; ++i) {
        QRect row(LEFT_MARGIN, y, plotArea.width(), rowH);
        QColor c = kRowColors[i % (sizeof(kRowColors) / sizeof(kRowColors[0]))];
        drawTrack(p, row, m_visibleCols[i], c);
        y += rowH + ROW_GAP;
    }

    // X axis ticks (time)
    p.setPen(palette().color(QPalette::Text));
    double dur = m_t1 - m_t0;
    double tickStep = 1000.0; // 1s
    if (dur > 30000) tickStep = 5000.0;
    if (dur > 60000) tickStep = 10000.0;
    if (dur < 5000)  tickStep = 500.0;
    int axisY = height() - BOTTOM_MARGIN + 2;
    p.drawLine(plotArea.left(), axisY, plotArea.right(), axisY);
    for (double tt = std::ceil(m_t0 / tickStep) * tickStep; tt <= m_t1; tt += tickStep) {
        int xx = m_timeToPx(tt, plotArea);
        p.drawLine(xx, axisY, xx, axisY + 4);
        p.drawText(xx - 30, axisY + 6, 60, 14, Qt::AlignHCenter,
                   QStringLiteral("%1 s").arg(tt / 1000.0, 0, 'f', 1));
    }

    // crosshair
    if (m_crosshairT >= m_t0 && m_crosshairT <= m_t1) {
        int cx = m_timeToPx(m_crosshairT, plotArea);
        QPen pen(QColor(120, 120, 120, 150), 1, Qt::DashLine);
        p.setPen(pen);
        p.drawLine(cx, TOP_MARGIN, cx, height() - BOTTOM_MARGIN);
    }
}

void TimeSeriesPlotWidget::drawTrack(QPainter &p, const QRect &row, int colIdx, const QColor &color)
{
    if (!m_t || colIdx < 0 || colIdx >= m_t->colCount()) return;
    p.setPen(palette().color(QPalette::Mid));
    p.drawRect(row);

    const QVector<double> &series = m_t->data[colIdx];
    const QVector<double> &time   = m_t->timeMs;
    if (series.size() < 2) return;

    // y range across visible window
    double y0 =  std::numeric_limits<double>::infinity();
    double y1 = -std::numeric_limits<double>::infinity();
    int firstIdx = -1, lastIdx = -1;
    for (int i = 0; i < series.size(); ++i) {
        if (time[i] < m_t0) continue;
        if (time[i] > m_t1) break;
        if (firstIdx < 0) firstIdx = i;
        lastIdx = i;
        if (series[i] < y0) y0 = series[i];
        if (series[i] > y1) y1 = series[i];
    }
    if (firstIdx < 0) return;
    if (y1 <= y0) y1 = y0 + 1.0;

    auto yPx = [&](double v) {
        double frac = (v - y0) / (y1 - y0);
        return row.bottom() - int(frac * row.height());
    };

    QPainterPath path;
    bool started = false;
    for (int i = firstIdx; i <= lastIdx; ++i) {
        int xx = m_timeToPx(time[i], QRect(row.left(), 0, row.width(), 1));
        int yy = yPx(series[i]);
        if (!started) { path.moveTo(xx, yy); started = true; }
        else          path.lineTo(xx, yy);
    }
    p.setPen(QPen(color, 1.4));
    p.drawPath(path);

    // label
    p.setPen(palette().color(QPalette::Text));
    QString label = m_t->columns[colIdx].name;
    if (!m_t->columns[colIdx].unitRaw.isEmpty())
        label += QStringLiteral(" [%1]").arg(m_t->columns[colIdx].unitRaw);
    p.drawText(row.left() - LEFT_MARGIN + 4, row.top() + 4, LEFT_MARGIN - 8, 14,
               Qt::AlignLeft | Qt::AlignTop, label);

    // axis numbers
    p.setPen(palette().color(QPalette::Mid));
    p.drawText(row.left() - LEFT_MARGIN + 4, row.top() + 18, LEFT_MARGIN - 8, 14,
               Qt::AlignLeft, QString::number(y1, 'f', 1));
    p.drawText(row.left() - LEFT_MARGIN + 4, row.bottom() - 14, LEFT_MARGIN - 8, 14,
               Qt::AlignLeft, QString::number(y0, 'f', 1));
}

void TimeSeriesPlotWidget::wheelEvent(QWheelEvent *e)
{
    if (!m_t) return;
    QRect plotArea(LEFT_MARGIN, TOP_MARGIN, width() - LEFT_MARGIN - RIGHT_MARGIN,
                   height() - TOP_MARGIN - BOTTOM_MARGIN);
    double mouseT = m_pxToTime(int(e->position().x()), plotArea);
    double scale  = (e->angleDelta().y() > 0) ? 0.8 : 1.25;
    double newSpan = (m_t1 - m_t0) * scale;
    if (newSpan < 50) newSpan = 50;
    double frac   = (mouseT - m_t0) / (m_t1 - m_t0);
    m_t0 = mouseT - frac * newSpan;
    m_t1 = m_t0 + newSpan;
    update();
    e->accept();
}

void TimeSeriesPlotWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_panning = true;
        m_panAnchorX = e->pos().x();
        m_panT0 = m_t0; m_panT1 = m_t1;
    }
}

void TimeSeriesPlotWidget::mouseMoveEvent(QMouseEvent *e)
{
    QRect plotArea(LEFT_MARGIN, TOP_MARGIN, width() - LEFT_MARGIN - RIGHT_MARGIN,
                   height() - TOP_MARGIN - BOTTOM_MARGIN);
    if (m_panning && plotArea.width() > 0) {
        double dt = double(e->pos().x() - m_panAnchorX) / double(plotArea.width());
        double span = m_panT1 - m_panT0;
        m_t0 = m_panT0 - dt * span;
        m_t1 = m_panT1 - dt * span;
        update();
    } else {
        m_crosshairT = m_pxToTime(e->pos().x(), plotArea);
        emit crosshairMoved(m_crosshairT);
        update();
    }
}

void TimeSeriesPlotWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) m_panning = false;
}

} // namespace datalog
