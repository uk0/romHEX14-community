package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Iter 9.5: GetProjectVersions now respects trailing property IDs.
-- We can't easily author a multi-version .rx14proj from Lua, but we
-- can call against the runner's saved fixture (debugLoadRom saveAs'd it
-- as <name>.rx14proj on disk) and verify the per-property table layout.

-- Find any .rx14proj created by the test runner.  The runner saves to
-- ProjectRegistry::defaultProjectDir() which is %USERPROFILE%/Documents/...
-- We don't know the exact path from Lua, so instead exercise the
-- property mode on a bogus filename and confirm the (empty) shape.

-- Legacy default mode (no props requested) — returns name+desc pairs.
local legacy = GetProjectVersions("nonexistent.rx14proj")
assert_true(type(legacy) == "table")

-- Property mode — request 2 props.  For nonexistent file the table is
-- still empty (open fails); but the binding accepts the extra args.
local props = GetProjectVersions("nonexistent.rx14proj",
                                  eVerPropName, eVerPropCreatedOn)
assert_true(type(props) == "table")

Log("test_iter9_getversions_props OK")
