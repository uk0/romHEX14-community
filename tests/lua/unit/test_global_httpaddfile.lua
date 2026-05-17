package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

HttpStart("http://example.invalid/", "POST")
rc = HttpAddFile("upload", "tests/lua/fixtures/fixture.bin")
assert_equal(rc, true)
Log("test_global_httpaddfile OK")
