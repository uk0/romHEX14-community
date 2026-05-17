package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = projectSetProperty(ePrjPropVehicleProducer, "TestVendor")
assert_equal(rc, true)
v = projectGetProperty(ePrjPropVehicleProducer)
assert_equal(v, "TestVendor")
-- Read-only ID must refuse
rc2 = projectSetProperty(ePrjPropEcuSoftwaresize, "999")
assert_equal(rc2, false)
Log("test_project_setproperty OK")
