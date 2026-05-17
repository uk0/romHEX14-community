package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- ePrjPropVehicleProducer was set earlier in test_project_setproperty
-- but we cannot rely on test order. Just check returns a string.
s = versionGetProperty(eVerPropName)
assert_true(type(s) == "string")
Log("test_version_getproperty OK")
