package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = projectSearchVehicleData()
assert_equal(rc, true)
Log("test_project_searchvehicledata OK")
