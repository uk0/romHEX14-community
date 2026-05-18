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
#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>
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

    // ── Sandbox (P0-1) ──────────────────────────────────────────────────
    //
    // The Lua stdlibs ship with several functions that give a malicious
    // script full RCE / data-exfiltration capability on the host machine:
    //
    //   CODE-EXEC (always blocked — no legit user-script use):
    //     - os.execute / io.popen → run arbitrary shell commands
    //     - os.exit → terminate the host application
    //     - package.loadlib → load and call any native .so/.dll/.dylib
    //     - dofile / loadfile / load → load arbitrary Lua bytecode from
    //                                  disk or from a string (can also
    //                                  crash the interpreter)
    //
    //   FILESYSTEM (path-guarded — legitimate use for temp files):
    //     - os.remove / os.rename / os.tmpname → tamper with the FS
    //   Replaced with versions that only accept paths inside the system
    //   temp directory OR under the current working directory (and never
    //   containing `..` segments).
    //
    // Scripts that need to read/write regular files can still use
    // io.open(); script composition is still possible via require() within
    // the package.path we control.
    {
        sol::table os_tbl  = L["os"];
        sol::table io_tbl  = L["io"];
        sol::table pkg_tbl = L["package"];

        // Path guard — returns canonicalised absolute path if safe, or
        // empty string if the path would escape the sandbox.
        auto guardPath = [](const std::string &raw) -> QString {
            QString p = QString::fromStdString(raw);
            if (p.isEmpty()) return QString();
            // Refuse parent-directory traversal at the syntactic level
            // even if Qt would canonicalise it away — defence in depth.
            const auto parts = p.split(QRegularExpression(
                QStringLiteral("[\\\\/]")), Qt::SkipEmptyParts);
            for (const auto &seg : parts) if (seg == QStringLiteral(".."))
                return QString();
            QFileInfo fi(p);
            const QString abs = fi.absoluteFilePath();
            const QString cwd = QDir::currentPath();
            const QString tmp = QDir::tempPath();
            const auto cs =
#ifdef Q_OS_WIN
                Qt::CaseInsensitive;
#else
                Qt::CaseSensitive;
#endif
            if (abs.startsWith(cwd, cs)) return abs;
            if (abs.startsWith(tmp, cs)) return abs;
            return QString();
        };

        if (os_tbl.valid()) {
            os_tbl["execute"] = sol::lua_nil;
            os_tbl["exit"]    = sol::lua_nil;
            os_tbl["remove"] = [guardPath](const std::string &path) -> bool {
                const QString safe = guardPath(path);
                if (safe.isEmpty()) return false;
                return QFile::remove(safe);
            };
            os_tbl["rename"] = [guardPath](const std::string &from,
                                           const std::string &to) -> bool {
                const QString f = guardPath(from);
                const QString t = guardPath(to);
                if (f.isEmpty() || t.isEmpty()) return false;
                return QFile::rename(f, t);
            };
            os_tbl["tmpname"] = []() -> std::string {
                // Create a real file in the system temp dir so subsequent
                // os.remove() (path-guarded above) accepts it.
                QString tpl = QDir::tempPath() + QStringLiteral("/rx14_XXXXXX");
                QFile f(tpl);
                if (!f.open(QIODevice::WriteOnly)) return std::string();
                const QString p = f.fileName();
                f.close();
                return p.toStdString();
            };
        }
        if (io_tbl.valid()) {
            io_tbl["popen"] = sol::lua_nil;
        }
        if (pkg_tbl.valid()) {
            pkg_tbl["loadlib"] = sol::lua_nil;
        }
        L["dofile"]   = sol::lua_nil;
        L["loadfile"] = sol::lua_nil;
        L["load"]     = sol::lua_nil;
    }

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
        QMessageBox::information(nullptr, QObject::tr("Lua script"), qtext);
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

    beginScript();
    sol::protected_function_result rc = L.safe_script_file(
        path.toStdString(), sol::script_pass_on_error);
    endScript();

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
    beginScript();
    sol::protected_function_result rc =
        m_impl->L.safe_script(chunk.toStdString(), sol::script_pass_on_error);
    endScript();
    if (!rc.valid()) {
        sol::error e = rc;
        if (err) *err = QString::fromUtf8(e.what());
        return false;
    }
    return true;
}

void LuaEngine::beginScript()
{
    if (++m_scriptDepth == 1) emit scriptRunningChanged(true);
}

void LuaEngine::endScript()
{
    if (m_scriptDepth > 0 && --m_scriptDepth == 0)
        emit scriptRunningChanged(false);
}

QString LuaEngine::takeOutput()
{
    QString o = m_output;
    m_output.clear();
    return o;
}

} // namespace lua
