package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Iter 10.2: GetLastError(false) returns a numeric stable hash of the
-- error message, 0 if no error.  GetLastError() / GetLastError(true)
-- returns the message string.
projectSearchChecksums()    -- triggers setLastError("checksum engine ...")
local s = GetLastError(true)
local n = GetLastError(false)
assert_true(type(s) == "string" and s:find("checksum") ~= nil)
assert_true(type(n) == "number" and n > 0,
            "expected positive numeric error code, got "..tostring(n))
Log("test_iter10_getlasterror_number OK n="..n)
