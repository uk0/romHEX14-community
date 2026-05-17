package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Iter 11.3: real-portal smoke.  Loads the actual portal client's
-- Lua scripts (`portal_find_options.lua` and `portal_apply_selection.lua`)
-- from a local checkout and verifies (a) they parse, (b) every API they
-- call is bound, (c) the contract portal depends on still holds.
--
-- Local checkout path comes from $ROMHEX14_PORTAL_LUA so the test files
-- in this repo stay free of personal paths.  Skips gracefully when unset.
--
-- We do not execute the scripts directly because they call Quit() on
-- missing config.txt which would kill the shared Lua engine.

local opt_path = env_path("ROMHEX14_PORTAL_LUA", "portal_find_options.lua")
local app_path = env_path("ROMHEX14_PORTAL_LUA", "portal_apply_selection.lua")

if opt_path then
    local fOpt, errOpt = loadfile(opt_path)
    assert_true(fOpt ~= nil, "portal_find_options.lua must parse: "..tostring(errOpt))
end
if app_path then
    local fApp, errApp = loadfile(app_path)
    assert_true(fApp ~= nil, "portal_apply_selection.lua must parse: "..tostring(errApp))
end

-- APIs portal_find_options uses.
for _, name in ipairs({"NewProject", "projectImport", "projectGetProperty",
                        "GetProjectVersions", "projectClose", "Log", "Quit"}) do
    assert_true(_G[name] ~= nil and type(_G[name]) == "function",
                name.." must be bound for portal_find_options")
end

-- APIs portal_apply_selection uses (subset of above + projectExport + projectImportChanges).
for _, name in ipairs({"projectImportChanges", "projectExport"}) do
    assert_true(_G[name] ~= nil and type(_G[name]) == "function",
                name.." must be bound for portal_apply_selection")
end

-- projectImportChanges contract: returns 0 ("source not found") on a bogus
-- path — what portal sees if its referenced .rx14proj is missing.  Pre-Iter-8
-- this returned 0 silently (no error message); Iter 6 made it real and
-- still returns 0 with GetLastError set.
local rc = projectImportChanges("nonexistent.rx14proj", 1)
assert_equal(rc, 0)

Log("test_portal_real OK"..(opt_path and "" or " (loadfile skipped)"))
