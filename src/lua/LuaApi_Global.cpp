/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Sprint L §5.1 — Global utility bindings (~33 non-HTTP functions).
 *
 * HTTP suite lands in Iter 3.
 */

#include "LuaEngine.h"
#include "LuaPropertyIds.h"
#include "mainwindow.h"
#include "project.h"

#include "sol/sol.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QString>
#include <QByteArray>
#include <QDebug>
#include <QSettings>
#include <QThread>
#include <QCryptographicHash>
#include "appconstants.h"
#include "version.h"

namespace lua {

namespace {

// ── helpers ────────────────────────────────────────────────────────────────

QString toQ(const std::string &s) { return QString::fromUtf8(s.c_str(), int(s.size())); }
std::string toS(const QString &q) { auto a = q.toUtf8(); return std::string(a.constData(), size_t(a.size())); }

// QVariant → sol::object (used by JSON conversion)
sol::object qVariantToSol(sol::state_view L, const QJsonValue &v);
QJsonValue solObjectToJson(const sol::object &v);

sol::object jsonValueToSol(sol::state_view L, const QJsonValue &v)
{
    switch (v.type()) {
    case QJsonValue::Bool:   return sol::make_object(L, v.toBool());
    case QJsonValue::Double: return sol::make_object(L, v.toDouble());
    case QJsonValue::String: return sol::make_object(L, toS(v.toString()));
    case QJsonValue::Array: {
        sol::table t = L.create_table();
        const QJsonArray a = v.toArray();
        for (int i = 0; i < a.size(); ++i)
            t[i + 1] = jsonValueToSol(L, a.at(i));
        return t;
    }
    case QJsonValue::Object: {
        sol::table t = L.create_table();
        const QJsonObject o = v.toObject();
        for (auto it = o.constBegin(); it != o.constEnd(); ++it)
            t[toS(it.key())] = jsonValueToSol(L, it.value());
        return t;
    }
    case QJsonValue::Null:
    case QJsonValue::Undefined:
    default:
        return sol::lua_nil;
    }
}

QJsonValue solObjectToJson(const sol::object &v)
{
    switch (v.get_type()) {
    case sol::type::boolean: return QJsonValue(v.as<bool>());
    case sol::type::number:  return QJsonValue(v.as<double>());
    case sol::type::string:  return QJsonValue(toQ(v.as<std::string>()));
    case sol::type::table: {
        sol::table t = v.as<sol::table>();
        // Detect array vs object: contiguous 1..N integer keys -> array
        bool isArray = true;
        std::size_t n = 0;
        for (auto &kv : t) {
            ++n;
            if (kv.first.get_type() != sol::type::number) { isArray = false; break; }
            double d = kv.first.as<double>();
            if (d != double(int(d))) { isArray = false; break; }
        }
        // Also need keys 1..n exactly:
        if (isArray) {
            for (std::size_t i = 1; i <= n; ++i) {
                sol::object o = t[i];
                if (!o.valid()) { isArray = false; break; }
            }
        }
        if (isArray) {
            QJsonArray arr;
            for (std::size_t i = 1; i <= n; ++i)
                arr.append(solObjectToJson(t[i]));
            return arr;
        }
        QJsonObject obj;
        for (auto &kv : t) {
            QString key;
            if (kv.first.get_type() == sol::type::string) key = toQ(kv.first.as<std::string>());
            else if (kv.first.get_type() == sol::type::number) key = QString::number(kv.first.as<double>());
            else continue;
            obj.insert(key, solObjectToJson(kv.second));
        }
        return obj;
    }
    case sol::type::lua_nil:
    default:
        return QJsonValue::Null;
    }
}

} // anonymous namespace

// ── public binding entry point ──────────────────────────────────────────────

void bindGlobalUtilities(sol::state &L, LuaEngine *engine)
{
    auto setLastErr = [engine](const QString &s) { engine->setLastError(s); };
    auto clrLastErr = [engine]() { engine->setLastError(QString()); };

    // 2.2.1/2  TextEntryDialog
    //
    // Old form (signature 1):
    //   (mode, title, desc, default="") → string
    //
    // New form (signature 2 — Iter 7 full impl):
    //   (title, mode1, desc1, default1, mode2, desc2, default2, ...) → table
    //
    // Per manual §2.2.2: up to 10 text inputs + 20 checkboxes per dialog.
    // We build a dynamic QDialog with one row per triplet and return an
    // array of the entered values (empty array on Cancel).
    L.set_function("TextEntryDialog", sol::overload(
        // Signature 1: old single-field form
        [](int mode, const std::string &title, const std::string &desc,
           sol::optional<std::string> def) -> std::string {
            QString d = def ? toQ(*def) : QString();
            QInputDialog dlg;
            dlg.setWindowTitle(toQ(title));
            dlg.setLabelText(toQ(desc));
            dlg.setTextValue(d);
            if (mode == 1 /* eTextEntryPassword */)
                dlg.setTextEchoMode(QLineEdit::Password);
            // Under test mode: skip the dialog, just return default value.
            if (qgetenv("RX14_LUA_TEST") == "1") return toS(d);
            if (dlg.exec() != QDialog::Accepted) return std::string();
            return toS(dlg.textValue());
        },
        // Signature 2: new multi-field form (Iter 7 full implementation)
        [&L](const std::string &title, sol::variadic_args va) -> sol::table {
            sol::table arr = L.create_table();
            // Triplets: (mode, desc, default).  Stop at non-triplet boundary.
            const int nTrips = int(va.size()) / 3;
            if (nTrips == 0) return arr;

            // Test mode: skip the modal dialog, return all defaults.
            if (qgetenv("RX14_LUA_TEST") == "1") {
                for (int i = 0; i < nTrips; ++i) {
                    QString def = toQ(va[i * 3 + 2].as<std::string>());
                    arr[i + 1] = toS(def);
                }
                return arr;
            }

            QDialog dlg;
            dlg.setWindowTitle(toQ(title));
            auto *form = new QFormLayout(&dlg);

            QVector<QLineEdit *> lineEdits;
            QVector<QCheckBox *> checkboxes;
            QVector<int>         modes;

            int textCount = 0, checkCount = 0;
            for (int i = 0; i < nTrips; ++i) {
                int     mode = va[i * 3 + 0].as<int>();
                QString desc = toQ(va[i * 3 + 1].as<std::string>());
                QString def  = toQ(va[i * 3 + 2].as<std::string>());
                modes.push_back(mode);
                if (mode == 3 /* eTextEntryCheckbox */) {
                    if (++checkCount > 20) break;   // manual cap
                    auto *cb = new QCheckBox(desc, &dlg);
                    cb->setChecked(def == QStringLiteral("1") || def.toLower() == QStringLiteral("true"));
                    form->addRow(cb);
                    checkboxes.push_back(cb);
                    lineEdits.push_back(nullptr);
                } else {
                    if (++textCount > 10) break;    // manual cap
                    auto *le = new QLineEdit(def, &dlg);
                    if (mode == 1 /* eTextEntryPassword */)
                        le->setEchoMode(QLineEdit::Password);
                    form->addRow(desc, le);
                    lineEdits.push_back(le);
                    checkboxes.push_back(nullptr);
                }
            }

            auto *buttons = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
            QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
            QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
            form->addRow(buttons);

            if (dlg.exec() != QDialog::Accepted) return arr;   // empty on cancel
            for (int i = 0; i < lineEdits.size(); ++i) {
                if (lineEdits[i])
                    arr[i + 1] = toS(lineEdits[i]->text());
                else if (checkboxes[i])
                    arr[i + 1] = std::string(checkboxes[i]->isChecked() ? "1" : "0");
            }
            return arr;
        }
    ));

    // 2.2.3  MessageBox  (overrides the one set in LuaEngine::initialize)
    L.set_function("MessageBox", [engine](const std::string &text,
                                          sol::optional<int> type) -> int {
        QString q = toQ(text);
        int t = type ? *type : 0;
        if (qgetenv("RX14_LUA_TEST") == "1") {
            engine->appendOutput(QStringLiteral("[MessageBox] %1\n").arg(q));
            return 1; // IDOK
        }
        // Iter 10.1: icon constants per WinAPI MB_ICON*.  Order matters
        // because the bits overlap: MB_ICONWARNING=0x30 contains 0x10 + 0x20.
        // Check the wider mask before the narrower one.
        QMessageBox::Icon icon = QMessageBox::NoIcon;
        const int iconBits = t & 0xF0;
        if      (iconBits == 0x10) icon = QMessageBox::Critical;       // MB_ICONERROR
        else if (iconBits == 0x20) icon = QMessageBox::Question;       // MB_ICONQUESTION
        else if (iconBits == 0x30) icon = QMessageBox::Warning;        // MB_ICONWARNING
        else if (iconBits == 0x40) icon = QMessageBox::Information;    // MB_ICONINFORMATION
        QMessageBox box(icon, QObject::tr("Lua script"), q);
        // Button set (low nibble)
        switch (t & 0xF) {
        case 1: box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel); break;
        case 2: box.setStandardButtons(QMessageBox::Abort | QMessageBox::Retry | QMessageBox::Ignore); break;
        case 3: box.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel); break;
        case 4: box.setStandardButtons(QMessageBox::Yes | QMessageBox::No); break;
        case 5: box.setStandardButtons(QMessageBox::Retry | QMessageBox::Cancel); break;
        default: box.setStandardButtons(QMessageBox::Ok); break;
        }
        int rc = box.exec();
        switch (rc) {
        case QMessageBox::Ok:     return 1;
        case QMessageBox::Cancel: return 2;
        case QMessageBox::Abort:  return 3;
        case QMessageBox::Retry:  return 4;
        case QMessageBox::Ignore: return 5;
        case QMessageBox::Yes:    return 6;
        case QMessageBox::No:     return 7;
        default:                  return 1;
        }
    });

    // 2.2.4 / 2.2.5  fromhex / tohex
    L.set_function("fromhex", [](const std::string &s) -> double {
        bool ok = false;
        qulonglong v = toQ(s).toULongLong(&ok, 16);
        return ok ? double(v) : 0.0;
    });
    L.set_function("tohex", [](sol::object n) -> std::string {
        qulonglong v = 0;
        if (n.get_type() == sol::type::number) v = qulonglong(n.as<double>());
        return toS(QString::number(v, 16).toUpper());
    });

    // 2.2.6 / 2.2.7  fromJSON / toJSON
    L.set_function("fromJSON", [&L](const std::string &s) -> sol::object {
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(s), &err);
        if (err.error != QJsonParseError::NoError) return sol::lua_nil;
        if (doc.isArray())  return jsonValueToSol(L, QJsonValue(doc.array()));
        if (doc.isObject()) return jsonValueToSol(L, QJsonValue(doc.object()));
        return sol::lua_nil;
    });
    L.set_function("toJSON", [](sol::object t) -> std::string {
        QJsonValue v = solObjectToJson(t);
        QJsonDocument doc;
        if (v.isArray())       doc = QJsonDocument(v.toArray());
        else if (v.isObject()) doc = QJsonDocument(v.toObject());
        else                   return std::string();
        return toS(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    });

    // 2.2.16-18  binaryor / binaryxor / binaryand
    L.set_function("binaryor", [](sol::variadic_args va) -> qulonglong {
        qulonglong r = 0;
        for (auto v : va) r |= qulonglong(v.as<double>());
        return r;
    });
    L.set_function("binaryxor", [](sol::variadic_args va) -> qulonglong {
        qulonglong r = 0;
        bool first = true;
        for (auto v : va) {
            qulonglong x = qulonglong(v.as<double>());
            if (first) { r = x; first = false; } else r ^= x;
        }
        return r;
    });
    L.set_function("binaryand", [](sol::variadic_args va) -> qulonglong {
        qulonglong r = ~qulonglong(0);
        bool first = true;
        for (auto v : va) {
            qulonglong x = qulonglong(v.as<double>());
            if (first) { r = x; first = false; } else r &= x;
        }
        return r;
    });

    // 2.2.19  Log  (overrides initialize() version with prettier output)
    L.set_function("Log", [engine](const std::string &s) {
        QString q = toQ(s);
        engine->appendOutput(q + QLatin1Char('\n'));
        qDebug().noquote() << "[lua Log]" << q;
    });

    // 2.2.20  Quit
    L.set_function("Quit", []() {
        if (qApp) qApp->quit();
    });

    // 2.2.21  SaveAll — Iter 7: iterate every open Project and call save()
    L.set_function("SaveAll", [engine]() {
        if (engine && engine->mainWindow())
            engine->mainWindow()->luaSaveAllProjects();
    });
    // 2.2.22  CloseAll — Iter 7: close every MDI sub-window
    L.set_function("CloseAll", [engine]() {
        if (engine && engine->mainWindow())
            engine->mainWindow()->luaCloseAllProjects();
    });

    // 2.2.23 / 2.2.24  SetClient / GetClient
    L.set_function("SetClient", sol::overload(
        []() -> std::string {
            return toS(QSettings(rx14::kOrgName, rx14::kAppName)
                           .value(QStringLiteral("lua/client")).toString());
        },
        [](const std::string &name) -> bool {
            QSettings(rx14::kOrgName, rx14::kAppName)
                .setValue(QStringLiteral("lua/client"), toQ(name));
            return true;
        }
    ));
    L.set_function("GetClient", [](sol::optional<bool> path) -> std::string {
        QString c = QSettings(rx14::kOrgName, rx14::kAppName)
                        .value(QStringLiteral("lua/client")).toString();
        if (path && *path) {
            // return path WITHOUT trailing separator per manual 2.2.24
            QString base = QDir::homePath() + QStringLiteral("/rx14-clients/") + c;
            while (base.endsWith(QLatin1Char('/')) || base.endsWith(QLatin1Char('\\')))
                base.chop(1);
            return toS(base);
        }
        return toS(c);
    });

    // 2.2.25  NewProject — Iter 6: create a real empty Project bound to
    // MainWindow so subsequent projectImport(path) actually populates it.
    // Required by portal_find_options.lua / portal_apply_selection.lua.
    L.set_function("NewProject", [engine]() {
        if (!engine || !engine->mainWindow()) return;
        engine->mainWindow()->luaNewProject();
    });

    // 2.2.26  GetVersion
    L.set_function("GetVersion", [](int id) -> int {
        using namespace lua_ids;
        switch (id) {
        case 1: return RX14_VERSION_MAJOR;
        case 2: return RX14_VERSION_MINOR;
        case 3: return ePluginMajor;
        case 4: return ePluginMinor;
        default: return 0;
        }
    });

    // 2.2.27  Sleep  (with optional file-watcher early-exit)
    L.set_function("Sleep", [](int ms, sol::optional<std::string> watch) {
        QElapsedTimer t; t.start();
        QString glob = watch ? toQ(*watch) : QString();
        QString dir, pat;
        if (!glob.isEmpty()) {
            int s = glob.lastIndexOf(QRegularExpression(QStringLiteral("[/\\\\]")));
            if (s > 0) {
                dir = glob.left(s);
                pat = glob.mid(s + 1);
            }
        }
        while (t.elapsed() < ms) {
            if (!dir.isEmpty() && !pat.isEmpty()) {
                QDir d(dir);
                if (!d.entryList({pat}, QDir::Files).isEmpty()) return;
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            QThread::msleep(10);
        }
    });

    // 2.2.28 / 2.2.29  FindProjects / FindProjects2  (Iter 4 will wire to ProjectRegistry)
    L.set_function("FindProjects", [&L](sol::variadic_args /*va*/) -> sol::table {
        return L.create_table();   // empty array — STUB-OK
    });
    L.set_function("FindProjects2", [&L](sol::variadic_args /*va*/) -> sol::table {
        return L.create_table();
    });

    // 2.2.30  DuplicateProject  (Iter 4)
    L.set_function("DuplicateProject", [setLastErr](const std::string &f) -> std::string {
        (void)f; setLastErr(QStringLiteral("DuplicateProject not yet implemented"));
        return std::string();
    });

    // 2.2.31  GetProjectVersions(filename, ...versionProperties) → table
    //
    // Iter 6: REAL — open .rx14proj from disk and return one row per
    //         version (including "Original" sentinel at row 0).
    // Iter 9.5: respect the trailing eVerProp* IDs.  Default mode (no props
    //         requested) keeps the legacy alternating name/description layout
    //         for portal_find_options.lua backwards compatibility.  When the
    //         caller passes eVerProp* IDs the table contains nProps values
    //         per version in argument order.
    L.set_function("GetProjectVersions", [&L, engine](sol::variadic_args va) -> sol::table {
        using namespace lua_ids;
        sol::table arr = L.create_table();
        if (va.size() < 1) return arr;
        QString filename = toQ(va[0].as<std::string>());
        QScopedPointer<Project> p(Project::open(filename));
        if (!p) {
            if (engine) engine->setLastError(QStringLiteral("cannot open %1").arg(filename));
            return arr;
        }

        // Collect requested property IDs (eVerProp*).
        QVector<int> propIds;
        for (size_t i = 1; i < va.size(); ++i) propIds.push_back(va[i].as<int>());

        // Helper: read one property from a ProjectVersion entry.  Index 0
        // is the "Original" sentinel; index >=1 maps to p->versions[idx-1].
        auto readVerProp = [&](int versionIdx, int propId) -> QString {
            if (versionIdx == 0) {
                switch (propId) {
                case eVerPropName:   return QStringLiteral("Original");
                case eVerPropComment:return QString();
                case eVerPropCreatedOn:
                case eVerPropChangedOn: return p->createdAt.toString(Qt::ISODate);
                default: return QString();
                }
            }
            if (versionIdx - 1 >= p->versions.size()) return QString();
            const ProjectVersion &v = p->versions[versionIdx - 1];
            switch (propId) {
            case eVerPropName:      return v.name;
            case eVerPropComment:   return QString();   // no description field
            case eVerPropCreatedOn: return v.created.toString(Qt::ISODate);
            case eVerPropChangedOn: return v.created.toString(Qt::ISODate);
            case eVerPropChecksum: {
                if (v.data.isEmpty()) return QString();
                return QString::fromLatin1(
                    QCryptographicHash::hash(v.data, QCryptographicHash::Md5).toHex());
            }
            default: return QString();
            }
        };

        int outIdx = 1;
        const int nVersions = 1 + p->versions.size();
        if (propIds.isEmpty()) {
            // Legacy mode — alternating name/description per version.
            for (int v = 0; v < nVersions; ++v) {
                arr[outIdx++] = toS(readVerProp(v, eVerPropName));
                arr[outIdx++] = toS(readVerProp(v, eVerPropComment));
            }
        } else {
            // Per-property mode — one cell per requested property per version.
            for (int v = 0; v < nVersions; ++v) {
                for (int id : propIds)
                    arr[outIdx++] = toS(readVerProp(v, id));
            }
        }
        return arr;
    });
    // 2.2.32  OpenProjectVersion(filename, versionIdx) → bool
    //
    // Iter 9.4: open a .rx14proj from disk and switch the active project's
    // currentData to the chosen version's snapshot.  versionIdx 0 maps to
    // originalData; 1..N maps to project->versions[idx-1].  Required by
    // EVC sample 07 "getversion" mode.
    L.set_function("OpenProjectVersion", [setLastErr, engine](sol::variadic_args va) -> bool {
        if (va.size() < 2) {
            setLastErr(QStringLiteral("OpenProjectVersion needs (filename, versionIdx)"));
            return false;
        }
        QString filename = toQ(va[0].as<std::string>());
        int versionIdx   = va[1].as<int>();
        QScopedPointer<Project> src(Project::open(filename));
        if (!src) {
            setLastErr(QStringLiteral("cannot open %1").arg(filename));
            return false;
        }
        QByteArray chosen;
        if (versionIdx <= 0) {
            chosen = src->originalData;
        } else if (versionIdx - 1 < src->versions.size()) {
            chosen = src->versions[versionIdx - 1].data;
        } else {
            setLastErr(QStringLiteral("version index %1 out of range (have %2)")
                       .arg(versionIdx).arg(src->versions.size()));
            return false;
        }
        if (chosen.isEmpty()) {
            setLastErr(QStringLiteral("version %1 has no data").arg(versionIdx));
            return false;
        }
        Project *active = engine ? engine->mainWindow()->luaActiveProject() : nullptr;
        if (!active) {
            setLastErr(QStringLiteral("no active project to receive version data"));
            return false;
        }
        active->currentData = chosen;
        if (active->originalData.isEmpty()) active->originalData = chosen;
        active->modified = true;
        emit active->dataChanged();
        return true;
    });

    // 2.2.33  DeleteProjectVersion(filename, versionIdx) → bool
    //
    // Iter 9.5: remove versions[idx-1] from the on-disk project file and
    // persist.  versionIdx 0 (original) cannot be deleted.
    L.set_function("DeleteProjectVersion", [setLastErr](sol::variadic_args va) -> bool {
        if (va.size() < 2) {
            setLastErr(QStringLiteral("DeleteProjectVersion needs (filename, versionIdx)"));
            return false;
        }
        QString filename = toQ(va[0].as<std::string>());
        int versionIdx   = va[1].as<int>();
        if (versionIdx <= 0) {
            setLastErr(QStringLiteral("cannot delete the original version (index 0)"));
            return false;
        }
        QScopedPointer<Project> p(Project::open(filename));
        if (!p) {
            setLastErr(QStringLiteral("cannot open %1").arg(filename));
            return false;
        }
        if (versionIdx - 1 >= p->versions.size()) {
            setLastErr(QStringLiteral("version index %1 out of range (have %2)")
                       .arg(versionIdx).arg(p->versions.size()));
            return false;
        }
        p->versions.removeAt(versionIdx - 1);
        return p->save();
    });

    // 2.2.34  OpenAndExport  (Iter 4)
    L.set_function("OpenAndExport", [setLastErr](sol::variadic_args /*va*/) -> bool {
        setLastErr(QStringLiteral("OpenAndExport not yet implemented"));
        return false;
    });

    // 2.2.35  ReactivateChecksums — STUB-OK
    L.set_function("ReactivateChecksums", []() { /* no-op */ });

    // 2.2.36  StartUrl
    L.set_function("StartUrl", [](const std::string &url) {
        QDesktopServices::openUrl(QUrl(toQ(url)));
    });

    // 2.2.37  ReadDirectory
    L.set_function("ReadDirectory", [&L](const std::string &pat) -> sol::table {
        sol::table arr = L.create_table();
        QString glob = toQ(pat);
        int s = glob.lastIndexOf(QRegularExpression(QStringLiteral("[/\\\\]")));
        QString dir = (s > 0) ? glob.left(s) : QStringLiteral(".");
        QString filter = (s > 0) ? glob.mid(s + 1) : glob;
        QDir d(dir);
        const auto entries = d.entryList({filter}, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        int i = 1;
        for (const QString &e : entries) arr[i++] = toS(e);
        return arr;
    });

    // 2.2.38  CreateDirectory  (0=error, 1=created, 2=already existed)
    L.set_function("CreateDirectory", [](const std::string &path) -> int {
        QString q = toQ(path);
        if (QFileInfo(q).exists()) return 2;
        return QDir().mkpath(q) ? 1 : 0;
    });

    // 2.2.39  MessagePump
    L.set_function("MessagePump", []() {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    });

    // 2.2.40  RemoveNonFilenameCharacters
    L.set_function("RemoveNonFilenameCharacters", [](const std::string &s) -> std::string {
        QString q = toQ(s);
        static const QRegularExpression rx(QStringLiteral("[\\\\/:*?\"<>|]"));
        q.remove(rx);
        return toS(q);
    });

    // 2.2.41  GetLastError(bAsString=true) → string | number
    //
    // Iter 10.2: manual says GetLastError(FALSE) returns a numeric error
    // code, GetLastError(TRUE) returns the message string.  We default to
    // string (matches pre-Iter-10 behaviour and most EVC samples), and
    // synthesize a stable 32-bit hash for the numeric form when asked
    // explicitly with `false`.
    L.set_function("GetLastError", [&L, engine](sol::optional<bool> asString) -> sol::object {
        const QString msg = engine->getLastError();
        const bool wantString = asString.value_or(true);
        if (wantString)
            return sol::make_object(L, toS(msg));
        if (msg.isEmpty())
            return sol::make_object(L, 0);
        return sol::make_object(L, int(qHash(msg) & 0x7FFFFFFF));
    });

    // 2.2.42  timeGetTime
    L.set_function("timeGetTime", []() -> qulonglong {
        static QElapsedTimer t;
        if (!t.isValid()) t.start();
        return qulonglong(t.elapsed());
    });

    // 2.2.43  requirex  (encrypted .luax not supported — try plain require)
    L.set_function("requirex", [&L, setLastErr](const std::string &name) -> bool {
        sol::protected_function req = L["require"];
        if (!req.valid()) return false;
        sol::protected_function_result rc = req(name);
        if (rc.valid()) return true;
        setLastErr(QStringLiteral("requirex: encrypted .luax not supported in community build"));
        return false;
    });

    // 2.2.44  usestrict  (both __index and __newindex per manual)
    L.set_function("usestrict", [&L]() {
        const char *strict = R"LUA(
            local mt = getmetatable(_G) or {}
            mt.__newindex = function(t, k, v)
                error("usestrict: undeclared global '"..tostring(k).."' (use declare to whitelist)", 2)
            end
            mt.__index = function(t, k)
                error("usestrict: read of undeclared global '"..tostring(k).."'", 2)
            end
            setmetatable(_G, mt)
        )LUA";
        L.script(strict);
    });

    // ── §5.6 compat helpers (auto-injected because samples 03-07 use them) ──
    const char *compat = R"LUA(
        function DoesFileExist(p)
            if p == nil then return false end
            local f = io.open(p, "rb")
            if f then f:close(); return true end
            return false
        end
        function chomp(s)
            if s == nil then return "" end
            return (string.gsub(s, "[\r\n]+$", ""))
        end
        function declare(name, value)
            rawset(_G, name, value)
        end
    )LUA";
    L.script(compat);
}

} // namespace lua
