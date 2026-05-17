package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- MessageBox under RX14_LUA_TEST=1 just logs and returns IDOK (=1)
rc = MessageBox("test message", MB_OK)
assert_equal(rc, 1)
Log("test_global_messagebox OK")
