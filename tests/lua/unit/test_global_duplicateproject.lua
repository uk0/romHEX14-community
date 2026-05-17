package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- STUB-FAIL
rc = DuplicateProject("nonexistent.ols")
assert_equal(rc, "")
Log("test_global_duplicateproject OK")
