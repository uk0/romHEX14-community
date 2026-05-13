#include "TimeSeriesPlotWidget.h"
#include "LogTable.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QLinearGradient>

#include <algorithm>
#include <cmath>
#include <limits>

namespace datalog {

namespace {
constexpr int LEFT_MARGIN   = 72;
constexpr int RIGHT_MARGIN  = 16;
constexpr int TOP_MARGIN    = 10;
constexpr int BOTTOM_MARGIN = 28;
constexpr int ROW_GAP       = 4;
constexpr int kNumColors    = 8;

const QColor kRowColors[kNumColors] = {
    QColor(56, 166, 255),   // vivid blue
    QColor(255, 85, 85),    // red
    QColor(80, 210, 120),   // green
    QColor(255, 190, 50),   // amber
    QColor(180, 120, 255),  // purple
    QColor(60, 210, 210),   // cyan
    QColor(255, 120, 180),  // pink
    QColor(170, 170, 170)   // grey
};

// Downsample: for each pixel bucket, emit min and max to preserve visual shape.
struct DownsamplePt { double x, y; };

QVector<DownsamplePt> downsample(const QVector<double> &series,
                                  const QVector<double> &time,
                                  int first, int last,
                                  double t0, double t1,
                                  int pxWidth)
{
    int n = last - first + 1;
    if (n <= 0) return {};
    if (n <= pxWidth * 3 || pxWidth <= 0) {
        QVector<DownsamplePt> out;
        out.reserve(n);
        for (int i = first; i <= last; ++i)
            out.append({time[i], series[i]});
        return out;
    }

    QVector<DownsamplePt> out;
    out.reserve(pxWidth * 2 + 2);
    double span = t1 - t0;
    if (span <= 0) span = 1.0;

    int prevBucket = -1;
    double bucketMin = 0, bucketMax = 0;
    double bucketMinT = 0, bucketMaxT = 0;
    bool bucketHasData = false;

    auto flush = [&]() {
        if (!bucketHasData) return;
        if (bucketMinT <= bucketMaxT) {
            out.append({bucketMinT, bucketMin});
            if (bucketMin != bucketMax)
                out.append({bucketMaxT, bucketMax});
        } else {
            out.append({bucketMaxT, bucketMax});
            if (bucketMin != bucketMax)
                out.append({bucketMinT, bucketMin});
        }
    };

    for (int i = first; i <= last; ++i) {
        int bucket = int((time[i] - t0) / span * pxWidth);
        bucket = std::clamp(bucket, 0, pxWidth - 1);

        if (bucket != prevBucket) {
            flush();
            bucketHasData = false;
            prevBucket = bucket;
        }

        double v = series[i];
        if (!bucketHasData) {
            bucketMin = bucketMax = v;
            bucketMinT = bucketMaxT = time[i];
            bucketHasData = true;
        } else {
            if (v < bucketMin) { bucketMin = v; bucketMinT = time[i]; }
            if (v > bucketMax) { bucketMax = v; bucketMaxT = time[i]; }
        }
    }
    flush();
    return out;
}

// Format a value smartly: fewer decimals for large values, more for small
QString smartFormat(double v)
{
    double abs = std::abs(v);
    if (abs >= 1000)  return QString::number(v, 'f', 0);
    if (abs >= 100)   return QString::number(v, 'f', 1);
    if (abs >= 1)     return QString::number(v, 'f', 2);
    return QString::number(v, 'f', 3);
}

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

double TimeSeriesPlotWidget::interpolateAt(int colIdx, double timeMs) const
{
    if (!m_t || colIdx < 0 || colIdx >= m_t->colCount()) return 0.0;
    const QVector<double> &t = m_t->timeMs;
    const QVector<double> &s = m_t->data[colIdx];
    if (t.isEmpty()) return 0.0;

    // Binary search for the sample just before timeMs
    auto it = std::lower_bound(t.begin(), t.end(), timeMs);
    int idx = int(it - t.begin());

    if (idx <= 0) return s.first();
    if (idx >= t.size()) return s.last();

    // Linear interpolation between idx-1 and idx
    double t0 = t[idx - 1], t1 = t[idx];
    double dt = t1 - t0;
    if (dt <= 0) return s[idx];
    double frac = (timeMs - t0) / dt;
    return s[idx - 1] + frac * (s[idx] - s[idx - 1]);
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

    // Per-track Y ranges stored for crosshair value drawing
    struct TrackInfo { QRect row; int colIdx; QColor color; double y0, y1; };
    QVector<TrackInfo> tracks;
    tracks.reserve(n);

    int y = TOP_MARGIN;
    for (int i = 0; i < n; ++i) {
        QRect row(LEFT_MARGIN, y, plotArea.width(), rowH);
        QColor c = kRowColors[i % kNumColors];
        drawTrack(p, row, m_visibleCols[i], c);

        // Recover the y-range that drawTrack computed (recompute cheaply here)
        const QVector<double> &series = m_t->data[m_visibleCols[i]];
        const QVector<double> &time = m_t->timeMs;
        int firstIdx = int(std::lower_bound(time.begin(), time.end(), m_t0) - time.begin());
        int lastIdx  = int(std::upper_bound(time.begin(), time.end(), m_t1) - time.begin()) - 1;
        if (firstIdx > 0) --firstIdx;
        if (lastIdx < series.size() - 1) ++lastIdx;
        firstIdx = std::clamp(firstIdx, 0, (int)series.size() - 1);
        lastIdx  = std::clamp(lastIdx, 0, (int)series.size() - 1);
        double vy0 = std::numeric_limits<double>::infinity();
        double vy1 = -std::numeric_limits<double>::infinity();
        for (int j = firstIdx; j <= lastIdx; ++j) {
            if (series[j] < vy0) vy0 = series[j];
            if (series[j] > vy1) vy1 = series[j];
        }
        double yPad = (vy1 - vy0) * 0.05;
        if (yPad < 0.5) yPad = 0.5;
        vy0 -= yPad; vy1 += yPad;
        if (vy1 <= vy0) vy1 = vy0 + 1.0;

        tracks.append({row, m_visibleCols[i], c, vy0, vy1});
        y += rowH + ROW_GAP;
    }

    // X axis ticks
    QFont axisFont = font();
    axisFont.setPointSize(8);
    p.setFont(axisFont);
    double dur = m_t1 - m_t0;
    double tickStep = 1000.0;
    if (dur > 120000)     tickStep = 30000.0;
    else if (dur > 60000) tickStep = 10000.0;
    else if (dur > 30000) tickStep = 5000.0;
    else if (dur > 10000) tickStep = 2000.0;
    else if (dur < 2000)  tickStep = 200.0;
    else if (dur < 5000)  tickStep = 500.0;
    int axisY = height() - BOTTOM_MARGIN + 2;
    p.setPen(QPen(palette().color(QPalette::Mid), 0.5));
    p.drawLine(plotArea.left(), axisY, plotArea.right(), axisY);

    QPen gridPen(palette().color(QPalette::Mid), 0.2, Qt::DotLine);
    for (double tt = std::ceil(m_t0 / tickStep) * tickStep; tt <= m_t1; tt += tickStep) {
        int xx = m_timeToPx(tt, plotArea);
        p.setPen(gridPen);
        p.drawLine(xx, TOP_MARGIN, xx, axisY);
        p.setPen(palette().color(QPalette::Text));
        p.drawLine(xx, axisY, xx, axisY + 4);
        QString label;
        if (dur > 60000)
            label = QStringLiteral("%1:%2")
                .arg(int(tt / 60000.0))
                .arg(int(std::fmod(tt / 1000.0, 60.0)), 2, 10, QLatin1Char('0'));
        else
            label = QStringLiteral("%1 s").arg(tt / 1000.0, 0, 'f', dur < 5000 ? 2 : 1);
        p.drawText(xx - 35, axisY + 5, 70, 16, Qt::AlignHCenter, label);
    }

    // Crosshair line + per-track value readout
    if (m_crosshairT >= m_t0 && m_crosshairT <= m_t1) {
        int cx = m_timeToPx(m_crosshairT, plotArea);
        p.setPen(QPen(QColor(200, 200, 200, 120), 1, Qt::DashLine));
        p.drawLine(cx, TOP_MARGIN, cx, height() - BOTTOM_MARGIN);

        // Draw value readout on each track
        for (const auto &ti : tracks) {
            drawCrosshairValue(p, ti.row, ti.colIdx, ti.color, ti.y0, ti.y1);
        }
    }
}

void TimeSeriesPlotWidget::drawTrack(QPainter &p, const QRect &row, int colIdx, const QColor &color)
{
    if (!m_t || colIdx < 0 || colIdx >= m_t->colCount()) return;

    // Subtle track background
    QColor bgColor = palette().color(QPalette::Base);
    QColor trackBg = bgColor.lighter(bgColor.lightness() > 128 ? 97 : 115);
    p.fillRect(row, trackBg);
    p.setPen(QPen(palette().color(QPalette::Mid), 0.5));
    p.drawRect(row);

    const QVector<double> &series = m_t->data[colIdx];
    const QVector<double> &time   = m_t->timeMs;
    if (series.size() < 2) return;

    // Binary search for visible range
    int firstIdx = int(std::lower_bound(time.begin(), time.end(), m_t0) - time.begin());
    int lastIdx  = int(std::upper_bound(time.begin(), time.end(), m_t1) - time.begin()) - 1;
    if (firstIdx > 0) --firstIdx;
    if (lastIdx < series.size() - 1) ++lastIdx;
    if (firstIdx > lastIdx || firstIdx >= series.size()) return;

    // Y range
    double y0 =  std::numeric_limits<double>::infinity();
    double y1 = -std::numeric_limits<double>::infinity();
    for (int i = firstIdx; i <= lastIdx; ++i) {
        if (series[i] < y0) y0 = series[i];
        if (series[i] > y1) y1 = series[i];
    }
    double yPad = (y1 - y0) * 0.05;
    if (yPad < 0.5) yPad = 0.5;
    double y0p = y0 - yPad, y1p = y1 + yPad;
    if (y1p <= y0p) y1p = y0p + 1.0;

    auto yPx = [&](double v) -> double {
        return row.bottom() - ((v - y0p) / (y1p - y0p)) * row.height();
    };

    // Horizontal grid lines
    p.setPen(QPen(palette().color(QPalette::Mid), 0.3, Qt::DotLine));
    for (int g = 1; g <= 3; ++g) {
        int gy = row.bottom() - int(g / 4.0 * row.height());
        p.drawLine(row.left(), gy, row.right(), gy);
    }

    // Downsample
    auto pts = downsample(series, time, firstIdx, lastIdx, m_t0, m_t1, row.width());
    if (pts.isEmpty()) return;

    // Build line path
    QPainterPath linePath;
    QRect coordRow(row.left(), 0, row.width(), 1);
    linePath.moveTo(m_timeToPx(pts[0].x, coordRow), yPx(pts[0].y));
    for (int i = 1; i < pts.size(); ++i)
        linePath.lineTo(m_timeToPx(pts[i].x, coordRow), yPx(pts[i].y));

    p.save();
    p.setClipRect(row);

    // Gradient fill
    {
        QPainterPath fillPath = linePath;
        fillPath.lineTo(m_timeToPx(pts.last().x, coordRow), row.bottom());
        fillPath.lineTo(m_timeToPx(pts.first().x, coordRow), row.bottom());
        fillPath.closeSubpath();

        QLinearGradient grad(0, row.top(), 0, row.bottom());
        QColor fillTop = color; fillTop.setAlpha(40);
        QColor fillBot = color; fillBot.setAlpha(4);
        grad.setColorAt(0.0, fillTop);
        grad.setColorAt(1.0, fillBot);
        p.setBrush(grad);
        p.setPen(Qt::NoPen);
        p.drawPath(fillPath);
    }

    // Line
    p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPath(linePath);
    p.restore();

    // ── Channel label in left margin ──
    QString label = m_t->columns[colIdx].name;
    if (!m_t->columns[colIdx].unitRaw.isEmpty())
        label += QStringLiteral(" [%1]").arg(m_t->columns[colIdx].unitRaw);
    QFont labelFont = p.font();
    labelFont.setPointSize(8);
    labelFont.setBold(true);
    p.setFont(labelFont);
    p.setPen(color);
    QRect labelRect(2, row.top(), LEFT_MARGIN - 6, row.height());
    p.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, label);

    // ── Y-axis min/max inside track (top-right and bottom-right, muted) ──
    QFont axisFont = p.font();
    axisFont.setPointSize(7);
    axisFont.setBold(false);
    p.setFont(axisFont);
    QColor dimText = palette().color(QPalette::Text);
    dimText.setAlpha(140);
    p.setPen(dimText);
    if (row.height() > 50) {
        p.drawText(row.left() + 4, row.top() + 1, row.width() - 8, 12,
                   Qt::AlignRight | Qt::AlignTop, smartFormat(y1));
        p.drawText(row.left() + 4, row.bottom() - 12, row.width() - 8, 12,
                   Qt::AlignRight | Qt::AlignBottom, smartFormat(y0));
    }
}

void TimeSeriesPlotWidget::drawCrosshairValue(QPainter &p, const QRect &row, int colIdx,
                                               const QColor &color, double y0, double y1)
{
    if (!m_t || m_crosshairT < m_t0 || m_crosshairT > m_t1) return;
    if (y1 <= y0) return;

    double val = interpolateAt(colIdx, m_crosshairT);

    // Y position of the value on this track
    double frac = (val - y0) / (y1 - y0);
    int dotY = row.bottom() - int(frac * row.height());
    dotY = std::clamp(dotY, row.top(), row.bottom());

    QRect coordRow(row.left(), 0, row.width(), 1);
    int dotX = m_timeToPx(m_crosshairT, coordRow);

    // Draw a dot at the intersection
    p.setBrush(color);
    p.setPen(QPen(Qt::white, 1.2));
    p.drawEllipse(QPoint(dotX, dotY), 4, 4);

    // Draw the value label with a background pill
    QString valText = smartFormat(val);
    if (!m_t->columns[colIdx].unitRaw.isEmpty())
        valText += QStringLiteral(" %1").arg(m_t->columns[colIdx].unitRaw);

    QFont valFont = p.font();
    valFont.setPointSize(8);
    valFont.setBold(true);
    p.setFont(valFont);
    QFontMetrics fm(valFont);
    int textW = fm.horizontalAdvance(valText) + 10;
    int textH = fm.height() + 4;

    // Position: to the right of the dot, unless near the right edge
    int labelX = dotX + 8;
    if (labelX + textW > row.right() - 4)
        labelX = dotX - textW - 8;

    int labelY = dotY - textH / 2;
    labelY = std::clamp(labelY, row.top() + 1, row.bottom() - textH - 1);

    // Background pill
    QRect pillRect(labelX, labelY, textW, textH);
    QColor pillBg = color;
    pillBg.setAlpha(200);
    p.setPen(Qt::NoPen);
    p.setBrush(pillBg);
    p.drawRoundedRect(pillRect, 4, 4);

    // Text
    p.setPen(Qt::white);
    p.drawText(pillRect, Qt::AlignCenter, valText);
}

void TimeSeriesPlotWidget::wheelEvent(QWheelEvent *e)
{
    if (!m_t) return;
    QRect plotArea(LEFT_MARGIN, TOP_MARGIN, width() - LEFT_MARGIN - RIGHT_MARGIN,
                   height() - TOP_MARGIN - BOTTOM_MARGIN);
    double span = m_t1 - m_t0;

    QPoint pxDelta  = e->pixelDelta();
    QPoint angDelta = e->angleDelta();

    if (!pxDelta.isNull()) {
        // Trackpad two-finger scroll → pan
        if (plotArea.width() > 0) {
            double dtX = -double(pxDelta.x()) / double(plotArea.width()) * span;
            double dtY = -double(pxDelta.y()) / double(plotArea.width()) * span;
            m_t0 += dtX + dtY;
            m_t1 += dtX + dtY;
        }
    } else if (angDelta.y() != 0) {
        // Mouse wheel → zoom
        double mouseT = m_pxToTime(int(e->position().x()), plotArea);
        double scale  = (angDelta.y() > 0) ? 0.8 : 1.25;
        double newSpan = span * scale;
        if (newSpan < 50) newSpan = 50;
        double maxSpan = m_t->timeMs.last() - m_t->timeMs.first();
        if (maxSpan > 0 && newSpan > maxSpan * 1.5) newSpan = maxSpan * 1.5;
        double frac = (mouseT - m_t0) / span;
        m_t0 = mouseT - frac * newSpan;
        m_t1 = m_t0 + newSpan;
    }

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

        // Emit per-channel values
        QVector<QPair<int,double>> vals;
        vals.reserve(m_visibleCols.size());
        for (int col : m_visibleCols)
            vals.append({col, interpolateAt(col, m_crosshairT)});
        emit crosshairValues(m_crosshairT, vals);

        update();
    }
}

void TimeSeriesPlotWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) m_panning = false;
}

} // namespace datalog
