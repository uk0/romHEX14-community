package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

v = projectGetChecksumOptionStatus(0)
assert_equal(v, 0)
Log("test_project_getchecksumoptionstatus OK")
