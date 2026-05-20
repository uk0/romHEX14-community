/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Sprint L §5.0.8 — Embedded Lua 5.4 + sol2 engine.
 *
 * Singleton accessed from MainWindow ("Datalog → Run Lua Script…") and from
 * DebugRpc cmdRunLua.  Lifetime spans application lifetime; initialize()
 * called once from MainWindow constructor.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QCoreApplication>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>

#include <optional>
#include <type_traits>
#include <utility>

class MainWindow;
class QThread;

namespace lua {

class LuaWorker;       // sol::state owner, lives on the Lua worker thread

class LuaEngine : public QObject {
    Q_OBJECT
public:
    static LuaEngine &instance();

    // One-time init: opens libraries, installs all bindings (§5.1–§5.4),
    // injects §5.6 compat helpers (DoesFileExist, chomp, declare).
    void initialize(MainWindow *mw);

    // Execute a .lua file synchronously. DebugRpc/tests use this path; GUI
    // actions should use runFileAsync() so the main event loop is not nested.
    //   { "ok": true|false, "output": "<captured Log() text>", "error": "<msg>" }
    QJsonObject runFile(const QString &path);
    void runFileAsync(const QString &path);

    // Execute an in-memory chunk (used internally to inject §5.6 helpers).
    bool runString(const QString &chunk, QString *err = nullptr);

    // The last error from any binding call.  Mirrors Lua's GetLastError().
    QString getLastError() const {
        QMutexLocker lock(&m_stateMutex);
        return m_lastError;
    }
    void setLastError(const QString &m) {
        QMutexLocker lock(&m_stateMutex);
        m_lastError = m;
    }

    // Drain accumulated Log() / print() output and reset buffer.
    QString takeOutput();
    void appendOutput(const QString &s) {
        QMutexLocker lock(&m_stateMutex);
        m_output.append(s);
    }

    MainWindow *mainWindow() const { return m_mw; }

    template <typename F>
    auto callOnGui(F &&fn) -> std::invoke_result_t<F>
    {
        using Ret = std::invoke_result_t<F>;
        QObject *target = QCoreApplication::instance();
        if (!target || QThread::currentThread() == target->thread()) {
            if constexpr (std::is_void_v<Ret>) {
                std::forward<F>(fn)();
                return;
            } else {
                return std::forward<F>(fn)();
            }
        }

        if constexpr (std::is_void_v<Ret>) {
            QMetaObject::invokeMethod(target, [&fn]() {
                fn();
            }, Qt::BlockingQueuedConnection);
            return;
        } else {
            std::optional<Ret> result;
            QMetaObject::invokeMethod(target, [&fn, &result]() {
                result.emplace(fn());
            }, Qt::BlockingQueuedConnection);
            return std::move(*result);
        }
    }

    // ── P0-4/5 busy-state guard ─────────────────────────────────────────
    //
    // Set to true while a script is executing on the Lua worker. MainWindow
    // uses this to gate UI actions that should not start overlapping scripts.
    bool isScriptRunning() const;
    void beginScript();
    void endScript();

signals:
    void scriptRunningChanged(bool running);
    void scriptFinished(const QJsonObject &result);

private:
    LuaEngine();
    ~LuaEngine() override;

    void clearRunState();

    QThread      *m_workerThread = nullptr;
    LuaWorker    *m_worker       = nullptr;
    MainWindow    *m_mw   = nullptr;
    mutable QMutex m_stateMutex;
    QString        m_output;
    QString        m_lastError;
    int            m_scriptDepth = 0;   // P0-4/5: re-entrant runFile/runString
};

} // namespace lua
