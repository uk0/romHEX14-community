package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Iter 5 promoted to REAL — binary export now succeeds.
outp = "tests/lua/fixtures/tmp_proj_export.bin"
os.remove(outp)
rc = projectExport(outp, eFiletypeBinary)
assert_equal(rc, true)
-- Gated types still STUB-FAIL
rc2 = projectExport("/tmp/x.vbf", eFiletypeVBF)
assert_equal(rc2, false)
Log("test_project_export OK")
