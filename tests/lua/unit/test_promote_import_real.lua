package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Round-trip: export → re-import → verify bytes survive.
exportpath = "tests/lua/fixtures/tmp_roundtrip.bin"
os.remove(exportpath)
projectSetAt(0x12345, 0xBE, eByte)
projectExport(exportpath, eFiletypeBinary)
assert_true(DoesFileExist(exportpath))

-- Modify currentData, then re-import — byte should be restored from file.
projectSetAt(0x12345, 0xFF, eByte)
assert_equal(projectGetAt(0x12345, eByte), 0xFF)
rc = projectImport(exportpath, eFiletypeBinary)
assert_equal(rc, true, "import should succeed")
assert_equal(projectGetAt(0x12345, eByte), 0xBE, "byte restored from import")

Log("test_promote_import_real OK")
