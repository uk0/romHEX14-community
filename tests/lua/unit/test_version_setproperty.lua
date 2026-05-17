package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = versionSetProperty(eVerPropName, "LuaTestVersion")
assert_equal(rc, true)
s = versionGetProperty(eVerPropName)
assert_equal(s, "LuaTestVersion")
Log("test_version_setproperty OK")
