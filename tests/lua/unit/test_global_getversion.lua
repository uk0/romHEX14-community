package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

major = GetVersion(eWinOLSMajor)
minor = GetVersion(eWinOLSMinor)
pmaj  = GetVersion(ePluginMajor)
assert_true(type(major) == "number")
assert_true(major >= 1, "major >= 1")
assert_equal(pmaj, 1)
Log("test_global_getversion OK")
