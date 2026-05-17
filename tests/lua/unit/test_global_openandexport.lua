package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = OpenAndExport("nonexistent.ols", 0, "/tmp/x.bin", eFiletypeBinary)
assert_equal(rc, false)
Log("test_global_openandexport OK")
