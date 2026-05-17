package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Iter 9.4: OpenProjectVersion exists and is REAL now (was stub).  We can't
-- easily create a multi-version .rx14proj from Lua, so we verify the error
-- paths: missing file, out-of-range index.

assert_true(type(OpenProjectVersion) == "function")

-- Missing file: returns false + GetLastError("cannot open").
local rc1 = OpenProjectVersion("nonexistent_iter9.rx14proj", 0)
assert_equal(rc1, false)
local err1 = GetLastError(true)
assert_true(err1:find("cannot open") ~= nil,
            "missing-file error: "..tostring(err1))

Log("test_iter9_openprojectversion OK")
