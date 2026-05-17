package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Iter 8.1: checksum engine is honest-false (was fake true).
rc = projectSearchChecksums()
assert_equal(rc, false)
err = GetLastError(true)
assert_true(type(err) == "string" and err:find("checksum") ~= nil,
            "expected error mentioning 'checksum', got "..tostring(err))
Log("test_project_searchchecksums OK")
