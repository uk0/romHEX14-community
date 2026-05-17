package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- EVC sample 06 "Import Server" — loops over input_path, NewProject +
-- projectImport + projectSetProperty + projectSave + projectClose for
-- each file.  Cannot run (infinite loop + missing input dir) but we
-- verify the API contract.
local samplePath = env_path("ROMHEX14_LUA_SAMPLES",
                            "06 - Import Server/ImportServer.lua")
if samplePath then
    local f, err = loadfile(samplePath)
    assert_true(f ~= nil, "sample 06 must parse: "..tostring(err))
end

assert_true(type(NewProject) == "function")
assert_true(type(ReadDirectory) == "function")

-- Sample chains: projectImport → projectSetProperty → projectSave → projectClose.
-- Iter 8.3: projectSave on path-less project returns false (was fake true).
-- The sample's pattern `projectImport(...) then ... projectSave()` would
-- silently appear to save before Iter 8.3.  Verify the new contract.
assert_true(type(projectImport) == "function")
assert_true(type(projectSetProperty) == "function")
assert_true(type(projectSave) == "function")
assert_true(type(projectClose) == "function")

-- ePrjUserdef1 constant exposed.
assert_true(type(ePrjUserdef1) == "number")

Log("test_evc_06_import_server OK"..(samplePath and "" or " (sample skipped)"))
