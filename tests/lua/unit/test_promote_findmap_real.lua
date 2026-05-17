package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Add a named map then locate it via projectFindMap
projectAddMap()
windowSetMapProperties("Name", "FindMe", TRUE)
windowSetMapProperties("Feldwerte.StartAddr", 0x9000, TRUE)

addr = projectFindMap("Name", "FindMe")
assert_equal(addr, 0x9000, "expected map address 0x9000")

-- Missing map → -1
missing = projectFindMap("Name", "NonexistentXYZ")
assert_equal(missing, -1)

Log("test_promote_findmap_real OK")
