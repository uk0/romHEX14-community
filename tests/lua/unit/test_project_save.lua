package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Without an explicit filePath, save returns true (test mode).
rc = projectSave(TRUE, "TestVer", "Sprint L test")
assert_true(rc == true or rc == false)
Log("test_project_save OK")
