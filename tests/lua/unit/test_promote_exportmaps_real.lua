package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Add a couple of maps then export to CSV + JSON
projectAddMap()
windowSetMapProperties("Name", "TestExportA", TRUE)
projectAddMap()
windowSetMapProperties("Name", "TestExportB", TRUE)

csvpath  = "tests/lua/fixtures/tmp_maps.csv"
jsonpath = "tests/lua/fixtures/tmp_maps.json"
os.remove(csvpath)
os.remove(jsonpath)
assert_equal(projectExportMaps(csvpath),  true)
assert_equal(projectExportMaps(jsonpath), true)
assert_true(DoesFileExist(csvpath))
assert_true(DoesFileExist(jsonpath))

Log("test_promote_exportmaps_real OK")
