package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = projectImportMapPack("/tmp/x.kp")
assert_equal(rc, false)
Log("test_project_importmappack OK")
