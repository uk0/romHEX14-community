package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Iter 6 promoted to REAL — non-existent source returns 0 (source not found).
rc = projectImportChanges("/tmp/nonexistent.rx14proj", 1)
assert_equal(rc, 0)
Log("test_project_importchanges OK")
