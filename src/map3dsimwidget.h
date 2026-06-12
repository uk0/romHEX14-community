/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QTimer>
#include "romdata.h"

class Map3DSimWidget : public QWidget {
    Q_OBJECT

public:
    explicit Map3DSimWidget(QWidget *parent = nullptr);

    void showMap(const QByteArray &romData, const MapInfo &map,
                 ByteOrder byteOrder);
    void clear();

signals:
    void cellHovered(int row, int col, double value);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct Vec3 { double x, y, z; };
    struct Pt2  { double x, y; };

    void rebuildGrid();
    Pt2  project(double x, double y, double z) const;
    QColor heatColor(double t) const;

    void drawSurface(QPainter &p);
    void drawGlassWalls(QPainter &p);
    void drawSlicePlanes(QPainter &p);
    void drawSurfaceIntersections(QPainter &p);
    void drawSliceCurves(QPainter &p);
    void drawCrosshair(QPainter &p);
    void drawAxes(QPainter &p);
    void drawValueLabel(QPainter &p);
    void drawMiniHeatmap(QPainter &p);
    void updateValueLabel();

    double interpZ(double normX, double normY) const;

    // ── Cursor-cell sync (3D surface ⇄ mini heatmap), issue #22 ─────────
    int  cursorRow() const;
    int  cursorCol() const;
    void setCursorCell(int row, int col);
    void setCursorNorm(double normX, double normY);
    bool pickSurfaceCell(const QPoint &pos, int &row, int &col) const;
    bool pickSurfacePos(const QPoint &pos, double *normX, double *normY) const;
    void selectFromMini(const QPoint &pos);
    QPointF crosshairScreenPos() const;
    const QVector<QPointF> &projectedVertices() const;

    // Data
    QByteArray m_data;
    MapInfo    m_map;
    ByteOrder  m_byteOrder = ByteOrder::BigEndian;
    bool       m_hasData = false;

    // Grid of physical values
    int m_cols = 0, m_rows = 0;
    QVector<QVector<double>> m_grid;   // [row][col]
    double m_minVal = 0, m_maxVal = 1;
    QVector<double> m_xAxis, m_yAxis;  // axis breakpoints

    // View parameters
    double m_rotX = -30;   // tilt angle (negative = looking down)
    double m_rotZ = 35;    // rotation around Z axis
    double m_zoom = 1.0;
    double m_panX = 0, m_panY = 0;
    static constexpr double kZScale = 1.05; // height scale for terrain

    // Crosshair
    double m_crossX = 0.5;  // 0..1 normalized position
    double m_crossY = 0.5;
    bool   m_crossDragging = false;

    // Interaction
    bool   m_rotating = false;
    QPoint m_lastMouse;

    // Cursor-cell sync state
    QRect  m_miniRect;             // heatmap area inside the minimap panel (set during paint)
    bool   m_miniDragging = false; // left-drag scrubbing on the minimap
    bool   m_maybeClick   = false; // press that hasn't turned into a rotate-drag yet
    QPoint m_pressPos;
    int    m_hoverRow = -1, m_hoverCol = -1;  // transient hover highlight (-1 = none)

    // Screen-space vertex cache so hover/click picking never re-projects the
    // grid per mouse-move. Invalidated on rotate/zoom/resize/grid changes.
    mutable QVector<QPointF> m_projCache;
    mutable bool m_projDirty = true;

    // Controls
    QSlider *m_sliderX = nullptr;
    QSlider *m_sliderY = nullptr;
    QLabel  *m_labelValue = nullptr;
    QLabel  *m_labelX = nullptr;
    QLabel  *m_labelY = nullptr;
};
