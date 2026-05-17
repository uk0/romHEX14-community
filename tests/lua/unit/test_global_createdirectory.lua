package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

tmpdir = "tests/lua/fixtures/tmp_create_test"
rc = CreateDirectory(tmpdir)
assert_true(rc == 1 or rc == 2, "should create (1) or exist (2)")
rc2 = CreateDirectory(tmpdir)
assert_equal(rc2, 2)
Log("test_global_createdirectory OK")
