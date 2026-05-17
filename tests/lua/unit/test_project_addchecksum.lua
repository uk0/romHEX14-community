package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = projectAddChecksum(0, 255, eHiLo)
assert_equal(rc, false)
Log("test_project_addchecksum OK")
