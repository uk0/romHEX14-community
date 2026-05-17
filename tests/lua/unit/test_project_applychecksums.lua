package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Iter 8.1: checksum engine is honest-false (was fake true).
rc = projectApplyChecksums()
assert_equal(rc, false)
Log("test_project_applychecksums OK")
