/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * LegionHarvestWorker — moves the voice harvest off the UI thread.
 *
 * Lives on a dedicated QThread.  Owns its SimilarityIndex connection,
 * reads each matching catalog file, runs the OLS importer, and builds
 * a LegionVoice for every qualifying voice.  Cooperative cancel via
 * `cancel()` (atomic flag checked between voices); the worker stops
 * after the current file finishes parsing.
 *
 * Progress is fanned out as (processed, total, currentPath) signals so
 * the UI can keep the user oriented without polling.
 */

#pragma once

#include "io/legion/LegionTypes.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVector>

#include <atomic>

namespace legion {

class LegionHarvestWorker : public QObject {
    Q_OBJECT
public:
    LegionHarvestWorker(QByteArray   userBaseline,
                        QString      selfPath,
                        int          minPercent,
                        int          maxVoices,
                        QObject     *parent = nullptr);

    /// Cooperative cancel.  Safe to call from any thread; the next
    /// voice-iteration boundary will honour it.
    void cancel() { m_cancel.store(true); }

signals:
    /// Emitted periodically as harvest progresses.  `total` is an upper
    /// bound from the catalog query; the worker may finish early when
    /// it hits the voice cap.
    void progress(int processed, int total, const QString &currentFile);

    /// Always emitted once.  `voices` is empty on cancel or no matches.
    /// `cancelled` is true iff `cancel()` was honoured.
    void finished(QVector<legion::LegionVoice> voices, bool cancelled);

public slots:
    void run();

private:
    QByteArray  m_baseline;
    QString     m_selfPath;
    int         m_minPercent = 85;
    int         m_maxVoices  = 30;
    std::atomic<bool> m_cancel{false};
};

}   // namespace legion
