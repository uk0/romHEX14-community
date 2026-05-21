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
#include <QEventLoop>
#include <QMetaObject>
#include <QThread>

#include <memory>

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

class LuaWorker : public QObject {
public:
    explicit LuaWorker(LuaEngine *engine) : m_engine(engine) {}

    void initializeState();
    QJsonObject runFile(const QString &path);
    bool runString(const QString &chunk, QString *err);

private:
    LuaEngine *m_engine = nullptr;
    std::unique_ptr<LuaEngineImpl> m_impl;
};

LuaEngine::LuaEngine() = default;
LuaEngine::~LuaEngine()
{
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
        delete m_workerThread;
        m_workerThread = nullptr;
        m_worker = nullptr;
    }
}

LuaEngine &LuaEngine::instance()
{
    static LuaEngine s;
    return s;
}

void LuaEngine::initialize(MainWindow *mw)
{
    if (m_worker) return;          // already initialized
    m_mw = mw;

    m_workerThread = new QThread;
    m_workerThread->setObjectName(QStringLiteral("LuaEngineWorker"));
    m_worker = new LuaWorker(this);
    m_worker->moveToThread(m_workerThread);
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_workerThread->start();

    QMetaObject::invokeMethod(m_worker, [this]() {
        m_worker->initializeState();
    }, Qt::BlockingQueuedConnection);
}

void LuaWorker::initializeState()
{
    if (m_impl) return;
    m_impl = std::make_unique<LuaEngineImpl>();
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
        m_engine->appendOutput(s);
        qDebug().noquote() << "[lua print]" << s.trimmed();
    });

    // Iter 1 minimal binding: MessageBox so end-to-end works on first run.
    // Full §5.1 surface comes in Iter 2 (LuaApi_Global.cpp).
    L.set_function("MessageBox", [this](const std::string &text) {
        QString qtext = QString::fromUtf8(text.c_str());
        // Suppress modal dialog under test mode (RX14_LUA_TEST=1).
        if (qgetenv("RX14_LUA_TEST") == "1") {
            m_engine->appendOutput(QStringLiteral("[MessageBox] %1\n").arg(qtext));
            return 1;     // IDOK per Sprint L spec §6.5
        }
        m_engine->callOnGui([&]() {
            QMessageBox::information(nullptr, QObject::tr("Lua script"), qtext);
        });
        return 1;
    });

    L.set_function("Log", [this](const std::string &s) {
        QString q = QString::fromUtf8(s.c_str());
        m_engine->appendOutput(q + QLatin1Char('\n'));
        qDebug().noquote() << "[lua Log]" << q;
    });

    // Iter 2 — full §5.1 + §5.5
    bindConstants(L);
    bindGlobalUtilities(L, m_engine);
    // Iter 3 — HTTP suite (8 funcs)
    bindHttpApi(L, m_engine);
    // Iter 4 — project context (real impls) + stubs (versions, maps, etc.)
    bindProjectApi(L, m_engine);
    bindStubApi(L, m_engine);
}

QJsonObject LuaWorker::runFile(const QString &path)
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

    auto &L = m_impl->L;

    sol::protected_function_result rc = L.safe_script_file(
        path.toStdString(), sol::script_pass_on_error);

    QString output = m_engine->takeOutput();
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

bool LuaWorker::runString(const QString &chunk, QString *err)
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

QJsonObject LuaEngine::runFile(const QString &path)
{
    QJsonObject result;
    if (!m_worker) {
        result.insert("ok", false);
        result.insert("error", QStringLiteral("LuaEngine not initialized"));
        return result;
    }
    if (isScriptRunning()) {
        result.insert("ok", false);
        result.insert("error", QStringLiteral("Lua script already running"));
        return result;
    }
    QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) {
        result.insert("ok", false);
        result.insert("error", QStringLiteral("file not found: %1").arg(path));
        return result;
    }

    clearRunState();
    beginScript();

    QObject *app = QCoreApplication::instance();
    if (QThread::currentThread() == m_workerThread) {
        result = m_worker->runFile(path);
    } else if (app && QThread::currentThread() == app->thread()) {
        QEventLoop loop;
        QMetaObject::invokeMethod(m_worker, [this, path, &result, &loop]() {
            result = m_worker->runFile(path);
            QMetaObject::invokeMethod(&loop, &QEventLoop::quit, Qt::QueuedConnection);
        }, Qt::QueuedConnection);
        loop.exec();
    } else {
        QMetaObject::invokeMethod(m_worker, [this, path, &result]() {
            result = m_worker->runFile(path);
        }, Qt::BlockingQueuedConnection);
    }

    endScript();
    return result;
}

void LuaEngine::runFileAsync(const QString &path)
{
    auto finishError = [this](const QString &message) {
        QJsonObject result;
        result.insert("ok", false);
        result.insert("error", message);
        emit scriptFinished(result);
    };

    if (!m_worker) {
        finishError(QStringLiteral("LuaEngine not initialized"));
        return;
    }
    if (isScriptRunning()) {
        finishError(QStringLiteral("Lua script already running"));
        return;
    }
    QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) {
        finishError(QStringLiteral("file not found: %1").arg(path));
        return;
    }

    clearRunState();
    beginScript();

    const bool queued = QMetaObject::invokeMethod(m_worker, [this, path]() {
        QJsonObject result = m_worker->runFile(path);
        QMetaObject::invokeMethod(this, [this, result]() {
            endScript();
            emit scriptFinished(result);
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);

    if (!queued) {
        endScript();
        finishError(QStringLiteral("failed to queue Lua script"));
    }
}

bool LuaEngine::runString(const QString &chunk, QString *err)
{
    if (!m_worker) {
        if (err) *err = QStringLiteral("LuaEngine not initialized");
        return false;
    }
    if (isScriptRunning()) {
        if (err) *err = QStringLiteral("Lua script already running");
        return false;
    }

    bool ok = false;
    QString workerErr;
    beginScript();

    QObject *app = QCoreApplication::instance();
    if (QThread::currentThread() == m_workerThread) {
        ok = m_worker->runString(chunk, &workerErr);
    } else if (app && QThread::currentThread() == app->thread()) {
        QEventLoop loop;
        QMetaObject::invokeMethod(m_worker, [this, chunk, &ok, &workerErr, &loop]() {
            ok = m_worker->runString(chunk, &workerErr);
            QMetaObject::invokeMethod(&loop, &QEventLoop::quit, Qt::QueuedConnection);
        }, Qt::QueuedConnection);
        loop.exec();
    } else {
        QMetaObject::invokeMethod(m_worker, [this, chunk, &ok, &workerErr]() {
            ok = m_worker->runString(chunk, &workerErr);
        }, Qt::BlockingQueuedConnection);
    }

    endScript();
    if (!ok && err) *err = workerErr;
    return ok;
}

bool LuaEngine::isScriptRunning() const
{
    bool running = false;
    auto check = [this, &running]() {
        running = m_scriptDepth > 0;
    };
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(const_cast<LuaEngine *>(this), check,
                                  Qt::BlockingQueuedConnection);
        return running;
    }
    check();
    return running;
}

void LuaEngine::beginScript()
{
    auto bump = [this]() {
        if (++m_scriptDepth == 1) emit scriptRunningChanged(true);
    };
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, bump, Qt::BlockingQueuedConnection);
        return;
    }
    bump();
}

void LuaEngine::endScript()
{
    auto drop = [this]() {
        if (m_scriptDepth > 0 && --m_scriptDepth == 0)
            emit scriptRunningChanged(false);
    };
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, drop, Qt::BlockingQueuedConnection);
        return;
    }
    drop();
}

void LuaEngine::clearRunState()
{
    QMutexLocker lock(&m_stateMutex);
    m_output.clear();
    m_lastError.clear();
}

QString LuaEngine::takeOutput()
{
    QMutexLocker lock(&m_stateMutex);
    QString o = m_output;
    m_output.clear();
    return o;
}

} // namespace lua
