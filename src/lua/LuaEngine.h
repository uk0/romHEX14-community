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

class MainWindow;

namespace lua {

class LuaEngineImpl;   // sol::state pimpl — keeps sol/sol.hpp out of headers

class LuaEngine : public QObject {
    Q_OBJECT
public:
    static LuaEngine &instance();

    // One-time init: opens libraries, installs all bindings (§5.1–§5.4),
    // injects §5.6 compat helpers (DoesFileExist, chomp, declare).
    void initialize(MainWindow *mw);

    // Execute a .lua file.  Returns:
    //   { "ok": true|false, "output": "<captured Log() text>", "error": "<msg>" }
    QJsonObject runFile(const QString &path);

    // Execute an in-memory chunk (used internally to inject §5.6 helpers).
    bool runString(const QString &chunk, QString *err = nullptr);

    // The last error from any binding call.  Mirrors Lua's GetLastError().
    QString getLastError() const { return m_lastError; }
    void    setLastError(const QString &m) { m_lastError = m; }

    // Drain accumulated Log() / print() output and reset buffer.
    QString takeOutput();
    void    appendOutput(const QString &s) { m_output.append(s); }

    MainWindow *mainWindow() const { return m_mw; }

    // ── P0-4/5 busy-state guard ─────────────────────────────────────────
    //
    // Set to true while a script is executing (incl. inside Sleep /
    // HttpExecute event-loop pumps).  MainWindow disables destructive
    // actions (Close Project, Quit, tab close) while this is true so
    // a re-entrant slot can't invalidate the raw Project* a binding is
    // holding.  Read-only from C++; bindings call beginScript/endScript.
    bool isScriptRunning() const { return m_scriptDepth > 0; }
    void beginScript();
    void endScript();

signals:
    void scriptRunningChanged(bool running);

private:
    LuaEngine();
    ~LuaEngine() override;

    LuaEngineImpl *m_impl = nullptr;   // sol::state holder
    MainWindow    *m_mw   = nullptr;
    QString        m_output;
    QString        m_lastError;
    int            m_scriptDepth = 0;   // P0-4/5: re-entrant runFile/runString
};

} // namespace lua
