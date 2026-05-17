package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- projectSave(true, "Lua-Snap") should call Project::snapshotVersion and
-- still return true (fixture has no filePath so disk save short-circuits
-- via the modified=false branch). We don't have a projectVersionCount API
-- exposed, so a non-throw + true is what we verify here.
local ok = projectSave(true, "Lua-Snap", "test")
assert_true(ok)

-- Calling without snapshot also works.
local ok2 = projectSave()
assert_true(ok2)

Log("test_promote_projectsave_snapshot OK")
