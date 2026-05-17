package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

assert_true(DoesFileExist("tests/lua/fixtures/fixture.bin"))
assert_equal(DoesFileExist("nonexistent_xyz_123.bin"), false)
Log("test_global_doesfileexist OK")
