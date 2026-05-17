package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Add a map with a known address, then delete by address.
projectAddMap()
windowSetMapProperties("Name", "DelByAddr", TRUE)
windowSetMapProperties("Feldwerte.StartAddr", 0xA000, TRUE)
rc = projectDelMap(0xA000)
assert_true(rc >= 1, "should delete >=1 map")

-- Add a couple matching a wildcard, then delete by pattern.
projectAddMap(); windowSetMapProperties("Name", "Pattern1_a", TRUE)
projectAddMap(); windowSetMapProperties("Name", "Pattern1_b", TRUE)
rc2 = projectDelMap("Pattern1_*")
assert_true(rc2 >= 2, "wildcard delete should remove >=2")

Log("test_promote_delmap_real OK")
