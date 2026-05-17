package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- EVC sample 03 "Simple server" — has an infinite `while(1)` loop so we
-- can't run end-to-end.  Instead we verify the script compiles cleanly
-- and every API it uses is callable with the documented contract.
-- Sample path comes from $ROMHEX14_LUA_SAMPLES; test skips loadfile()
-- when unset so the suite is portable across machines.

local samplePath = env_path("ROMHEX14_LUA_SAMPLES",
                            "03 - Simple server/server.lua")
if samplePath then
    local f, err = loadfile(samplePath)
    assert_true(f ~= nil, "sample 03 must parse: "..tostring(err))
end

-- APIs used by the sample.
assert_true(type(Sleep)          == "function")
assert_true(type(MessagePump)    == "function")
assert_true(type(DoesFileExist)  == "function")
assert_true(type(MessageBox)     == "function")
assert_true(type(binaryor)       == "function")

-- Constants used.
assert_true(type(MB_ICONQUESTION) == "number")
assert_true(type(MB_OKCANCEL)     == "number")
assert_true(type(IDCANCEL)        == "number")

-- binaryor must combine flags.
assert_true(binaryor(MB_ICONQUESTION, MB_OKCANCEL) > 0)

Log("test_evc_03_simple_server OK"..(samplePath and "" or " (sample skipped)"))
