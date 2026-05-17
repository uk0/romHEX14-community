package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

v = projectAddrRawToCpu(0x100)
assert_equal(v, 0x100)
Log("test_project_addrrawtocpu OK")
