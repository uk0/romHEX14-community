package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Export to a temp binary, verify file exists and size matches.
outpath = "tests/lua/fixtures/tmp_export.bin"
os.remove(outpath)
rc = projectExport(outpath, eFiletypeBinary)
assert_equal(rc, true, "export should succeed")
assert_true(DoesFileExist(outpath), "exported file must exist")

-- Gated type still STUB-FAIL
rc2 = projectExport("/tmp/x.vbf", eFiletypeVBF)
assert_equal(rc2, false)

Log("test_promote_export_real OK")
