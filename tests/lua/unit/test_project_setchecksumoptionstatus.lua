package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = projectSetChecksumOptionStatus(0, 1)
assert_equal(rc, false)
Log("test_project_setchecksumoptionstatus OK")
