/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Sprint L Iter 1 — LuaEngine skeleton.
 *
 * Wires sol2 over Lua 5.4, opens standard libraries, exposes a placeholder
 * MessageBox so `Datalog → Run Lua Script…` can demonstrate end-to-end.
 *
 * Iter 2+ will progressively bind the full §5.1–§5.4 surface from the
 * other LuaApi_*.cpp files.
 */

#include "LuaEngine.h"

// Forward-declare the binding entry points implemented in the other
// LuaApi_*.cpp files.  Iter 1 only has globals stubbed in; later iters
// implement the rest.
#include "sol/sol.hpp"

#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QApplication>
#include <QDebug>

namespace lua {

// Declared in LuaApi_*.cpp files and called from initialize() below.
extern void bindGlobalUtilities(sol::state &L, LuaEngine *engine);
extern void bindConstants(sol::state &L);
extern void bindHttpApi(sol::state &L, LuaEngine *engine);
extern void bindProjectApi(sol::state &L, LuaEngine *engine);
extern void bindStubApi(sol::state &L, LuaEngine *engine);

// pimpl so the public header doesn't need sol/sol.hpp
class LuaEngineImpl {
public:
    sol::state L;
};

LuaEngine::LuaEngine() = default;
LuaEngine::~LuaEngine() { delete m_impl; }

LuaEngine &LuaEngine::instance()
{
    static LuaEngine s;
    return s;
}

void LuaEngine::initialize(MainWindow *mw)
{
    if (m_impl) return;            // already initialized
    m_mw = mw;
    m_impl = new LuaEngineImpl;
    auto &L = m_impl->L;

    L.open_libraries(
        sol::lib::base,
        sol::lib::string,
        sol::lib::math,
        sol::lib::table,
        sol::lib::io,
        sol::lib::os,
        sol::lib::package
    );

    // Redirect Lua's `print` to our captured-output buffer.
    L.set_function("print", [this](sol::variadic_args va) {
        QString s;
        for (auto v : va) {
            if (!s.isEmpty()) s.append(QLatin1Char('\t'));
            s.append(QString::fromUtf8(sol::state_view(va.lua_state())
                .get<sol::function>("tostring")(v)
                .get<std::string>().c_str()));
        }
        s.append(QLatin1Char('\n'));
        appendOutput(s);
        qDebug().noquote() << "[lua print]" << s.trimmed();
    });

    // Iter 1 minimal binding: MessageBox so end-to-end works on first run.
    // Full §5.1 surface comes in Iter 2 (LuaApi_Global.cpp).
    L.set_function("MessageBox", [this](const std::string &text) {
        QString qtext = QString::fromUtf8(text.c_str());
        // Suppress modal dialog under test mode (RX14_LUA_TEST=1).
        if (qgetenv("RX14_LUA_TEST") == "1") {
            appendOutput(QStringLiteral("[MessageBox] %1\n").arg(qtext));
            return 1;     // IDOK per Sprint L spec §6.5
        }
        QMessageBox::information(nullptr, QStringLiteral("Lua"), qtext);
        return 1;
    });

    L.set_function("Log", [this](const std::string &s) {
        QString q = QString::fromUtf8(s.c_str());
        appendOutput(q + QLatin1Char('\n'));
        qDebug().noquote() << "[lua Log]" << q;
    });

    // Iter 2 — full §5.1 + §5.5
    bindConstants(L);
    bindGlobalUtilities(L, this);
    // Iter 3 — HTTP suite (8 funcs)
    bindHttpApi(L, this);
    // Iter 4 — project context (real impls) + stubs (versions, maps, etc.)
    bindProjectApi(L, this);
    bindStubApi(L, this);
}

QJsonObject LuaEngine::runFile(const QString &path)
{
    QJsonObject result;
    if (!m_impl) {
        result.insert("ok", false);
        result.insert("error", QStringLiteral("LuaEngine not initialized"));
        return result;
    }
    QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) {
        result.insert("ok", false);
        result.insert("error", QStringLiteral("file not found: %1").arg(path));
        return result;
    }

    m_output.clear();
    m_lastError.clear();
    auto &L = m_impl->L;

    sol::protected_function_result rc = L.safe_script_file(
        path.toStdString(), sol::script_pass_on_error);

    QString output = takeOutput();
    if (!rc.valid()) {
        sol::error err = rc;
        QString errStr = QString::fromUtf8(err.what());
        result.insert("ok", false);
        result.insert("output", output);
        result.insert("error", errStr);
        return result;
    }
    result.insert("ok", true);
    result.insert("output", output);
    result.insert("error", QString());
    return result;
}

bool LuaEngine::runString(const QString &chunk, QString *err)
{
    if (!m_impl) {
        if (err) *err = QStringLiteral("LuaEngine not initialized");
        return false;
    }
    sol::protected_function_result rc =
        m_impl->L.safe_script(chunk.toStdString(), sol::script_pass_on_error);
    if (!rc.valid()) {
        sol::error e = rc;
        if (err) *err = QString::fromUtf8(e.what());
        return false;
    }
    return true;
}

QString LuaEngine::takeOutput()
{
    QString o = m_output;
    m_output.clear();
    return o;
}

} // namespace lua
