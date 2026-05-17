package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- SaveAll now iterates m_projects. With an unsaved fixture-loaded project
-- (no filePath set), it skips and returns 0 saves silently. We just verify
-- the call doesn't throw and the active project survives.
assert_true(type(SaveAll) == "function")
SaveAll()

-- Project still present after the call.
sz = projectGetProperty(ePrjPropEcuSoftwaresize)
assert_equal(sz, "2097152")
Log("test_promote_saveall_real OK")
