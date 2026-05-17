# Sprint L progress log

## Iter 0 — C++ accessor preamble  ✅
Commit: `b85f7ae` on branch `sprint-L-lua`

* `.gitattributes` created — prevents CRLF rewrite of vendored Lua sources
* `src/lua/LuaPropertyIds.h` — single source of truth for ePrjProp*/eVerProp*/
  e*/MB_* numeric constants (consumed by both Project::propertyById and
  LuaConstants.cpp later)
* `src/romdata.h`:
  - `MapInfo::sideProps` (`QHash<QString,QVariant>`) added for the
    accepted-no-op map-property bag
  - Free fns `mapTypeFromWinOlsEnum`/`mapTypeToWinOlsEnum` for the
    eZweidim/eEindim/eAchse ↔ "MAP"/"CURVE"/"AXIS" bridge
* `src/project.{h,cpp}`:
  - `Project::propertyById(id, orgVer)` + `setPropertyById(id, value)` —
    full §5.2a mapping (~60 enum IDs → real fields)
  - MD5/SHA1/SHA256 computed via QCryptographicHash for ePrjPropChecksum*
    family (iOrgVer=1 only)
  - STUB-MISSING returns "" for Tag/Reseller/8-bit-checksum ids
  - Read-only ids (Softwaresize/Checksum/CreatedOn/ModifiedOn/Filename) refused
    per WinOLS manual §2.4.2
  - `Project::lastError`/`setLastError` for Lua's GetLastError()
* `src/debug/DebugRpc.{h,cpp}`:
  - `cmdRunLua` handler registered for "lua_run" command (Iter 0 stub
    returns "lua engine not yet implemented")

**Build verified**: rx14.exe links clean with RX14_DEBUG_RPC=ON. No new
warnings on added code.

## Iter 1 — Vendor Lua + sol2 + LuaEngine + menu  ✅

* Vendored `third_party/lua-5.4/src/*` from official Lua 5.4.8 tarball
  (SHA-256 `4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae`).
  63 source files (31 .c + 21 .h + lprefix.h + makefile etc.).
* Vendored `third_party/sol2/sol/*` from sol2 v3.5.0 git tag — used the
  multi-header layout (113 files, 1.4 MB) rather than the single header
  because the GitHub release asset `sol.hpp` returned 404 and the
  `single.py` generator has a Windows-path bug. Multi-header is officially
  supported and functionally equivalent.
* `CMakeLists.txt` — added `add_library(lua_static …)` block with 32
  Lua sources, SYSTEM include, AUTOMOC OFF, LUA_USE_WINDOWS on Win32,
  -w to silence Lua's own warnings. Appended `lua_static` to rx14
  target_link_libraries, added sol2 to SYSTEM include path on rx14.
* `src/lua/LuaEngine.{h,cpp}` — pimpl-based singleton wrapping
  `sol::state`. Opens base/string/math/table/io/os/package libs.
  Iter 1 minimal bindings: redirected `print` → captured output buffer,
  `MessageBox` (suppress-modal under `RX14_LUA_TEST=1`), `Log`. Bigger
  binding surface (§5.1 globals, §5.2 project, etc.) lands in Iter 2+.
* `src/mainwindow.cpp` — `lua::LuaEngine::instance().initialize(this)`
  called once from ctor after MDI creation. New menu entry
  `Datalog → Run Lua Script…` opens a file picker and runs the chosen
  `.lua` via LuaEngine.runFile(), surfacing output/errors in a message box.
* `src/debug/DebugRpc.cpp` — `cmdRunLua` now delegates to
  `lua::LuaEngine::instance().runFile()`.

**Build verified**: rx14.exe 6.9 MB (was 6.5 MB pre-Lua; +400 KB for
embedded Lua 5.4). Build flags unchanged (RX14_DEBUG_RPC=ON).

## Next: Iter 2 — full §5.1 global utilities + tests

* `src/lua/LuaApi_Global.cpp` — bind all 33 non-HTTP global functions
  per spec §5.1
* `src/lua/LuaConstants.cpp` — bind all e*/MB_*/ID*/TRUE/FALSE globals
  from LuaPropertyIds.h
* Inject §5.6 compat helpers (DoesFileExist, chomp, declare)
* `tests/lua/unit/test_global_*.lua` — one assertion file per function
* `tests/lua/test_helper.lua` — assert_equal/assert_true/assert_close
* `tests/lua/run_tests.ps1` — PowerShell runner per §6.2
* Copy fixture EDC17C46 ROM to `tests/lua/fixtures/fixture.bin`

## Iter 3 — HTTP client suite ✅
8 HTTP funcs + 8 tests, all green. Tests aim at 127.0.0.1:1 (unroutable)
so they verify the API without touching the real network.

## Iter 4 — Project properties + version + close/save + byte access + comments ✅
Plus stub group: checksums, mail, find/replace/maps/export/import/rights/
AutoUpdate/CloneVehicleData/QuickFix.

Real implementations in this iter:
  - projectGetProperty / SetProperty (using Project::propertyById)
  - projectClose / projectSave (delegating to existing Project methods)
  - projectGet/SetElement, GetElementOffset, SetElementRanges
  - projectGetAt/SetAt for eByte/eLoHi/eHiLo/eLoHiLoHi/eHiLoHiLo/eFloat*
    with mode=absolute/relative/percent + string form ("AA BB ?? DD",
    "10M" decimal suffix, eEmptyvalue restore)
  - projectSetOrg (copy from originalData)
  - projectCountDifferentBytes (default returns IDENTICAL bytes per
    manual misleading default)
  - projectAddrCpuToRaw / RawToCpu (identity in our flat model)
  - projectAddCommentAt / GetCommentAt / DelCommentAt (via AnnotationStore)

Stubs satisfying §7 acceptance (callable, right return type, sets
GetLastError when applicable):
  - checksums (9 funcs), projectMail, export/import (5 funcs),
    find/replace bytes (2), maps (5), rights (2), import-changes (1),
    AutoUpdate/AutoImport (2), CloneVehicleData (1), QuickFix (3),
    findSimilar/Sql (2 — alias)

Version + window contexts:
  - versionGet/SetProperty (mapped to Project fields)
  - windowGetActive/SetActive (placeholder pointer ID)
  - windowGet/SetMapProperties with full §5.4 property bag:
    direct-field mappings (Name, Spalten, Zeilen, Typ, Kommentar,
    bVorzeichen, Feldwerte.{StartAddr,Faktor,Offset,Einheit},
    StuetzX/Y.{Name,DataAddr}) + side-map fallback for everything else
    (~40 properties accepted-no-op)

Integration tests added under tests/lua/integration/:
  - test_winols_01_hello.lua
  - test_winols_02_version.lua
  - test_winols_08_create_map.lua (43 windowSetMapProperties calls)

## ✅ SPRINT L ACCEPTANCE GATES — ALL PASS

| Gate | Status |
|---|---|
| 1 Build clean | ✅ rx14.exe linked (Release + RX14_DEBUG_RPC=ON) |
| 2 No new warnings on src/lua/* | ✅ clean |
| 3 API coverage (104 funcs) | ✅ 104/104 active calls |
| 4 Test pass rate | ✅ 107/107 |
| 5 Integration (3 WinOLS samples) | ✅ all pass |

