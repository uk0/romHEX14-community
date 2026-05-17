package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = projectSetRight(ePRExportKp, false)
assert_equal(rc, false)
Log("test_project_setright OK")
