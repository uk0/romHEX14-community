package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- EVC sample 05 "Advanced Server" — checksum-name reporting loop with
-- infinite outer while(1).  Syntax check (when sample is on disk) +
-- API contract.
local samplePath = env_path("ROMHEX14_LUA_SAMPLES",
                            "05 - Advanced Server/AdvancedServer.lua")
if samplePath then
    local f, err = loadfile(samplePath)
    assert_true(f ~= nil, "sample 05 must parse: "..tostring(err))
end

-- Sample relies on projectSearchChecksums then checks
-- `if (projectStatChecksums()>-1)` before reading checksum names.
-- Our Iter 8.1: searchChecksums returns false, statChecksums returns -1,
-- so the sample's branch correctly skips name reads.
assert_equal(projectSearchChecksums(), false)
assert_equal(projectStatChecksums(),   -1)

-- projectGetChecksumName returns "" (no engine).
local name = projectGetChecksumName()
assert_equal(name, "")

-- projectClose(TRUE) callable (Iter 8.2 delegates via eventFilter).
assert_true(type(projectClose) == "function")

Log("test_evc_05_advanced_server OK"..(samplePath and "" or " (sample skipped)"))
