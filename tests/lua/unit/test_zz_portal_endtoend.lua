package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Portal end-to-end smoke: exercises every function that
-- portal_find_options.lua and portal_apply_selection.lua use,
-- without needing a populated WinOLS client database.

-- §5.1 NewProject — must actually create a project so projectImport
-- can populate it.  Iter 6 promoted from no-op to real.
assert_true(type(NewProject) == "function")
NewProject()

-- §2.4.7 projectImport — Iter 5 promoted to REAL for binary.
-- We re-import our own fixture into the freshly created project.
rc = projectImport("tests/lua/fixtures/fixture.bin", eFiletypeBinary)
-- A NewProject() right above means currentData is empty so the
-- "size match" guard accepts any incoming binary.
assert_equal(rc, true, "projectImport must succeed on fresh project")

-- §2.4.1 projectGetProperty — should be callable on the new project
sw = projectGetProperty(ePrjPropEcuSoftwareversion)
assert_true(type(sw) == "string")

-- §2.2.31 GetProjectVersions — Iter 6 promoted to REAL.
-- Returns at least the "Original" baseline (2 entries: name + desc)
-- even for a brand new project with no extra versions.
versions = GetProjectVersions("nonexistent.rx14proj")
assert_true(type(versions) == "table")

-- §2.4.31 projectFindSimilarProjectsSql — Iter 6 promoted to REAL.
-- Without a populated SimilarityIndex DB it returns an empty table,
-- which is the expected behavior on a clean machine.
matches = projectFindSimilarProjectsSql(-85, 100, eFSPTrippleRelevance,
    ePrjFilename, ePrjPropVehicleProducer)
assert_true(type(matches) == "table")

-- §2.4.45 projectImportChanges — Iter 6 promoted to REAL.
-- Returns 0 ("source not found") on bogus path.
rc2 = projectImportChanges("nonexistent.rx14proj", 1)
assert_equal(rc2, 0)

-- §2.4.40 projectCountDifferentBytes — Iter 4 REAL
diff = projectCountDifferentBytes(TRUE)
assert_true(type(diff) == "number")

-- §2.4.5 projectExport — Iter 5 REAL for binary
outp = "tests/lua/fixtures/tmp_portal_export.bin"
os.remove(outp)
ok = projectExport(outp, eFiletypeBinary)
assert_equal(ok, true)
assert_true(DoesFileExist(outp))

-- §2.4.3 projectClose(TRUE) — Iter 6 honors bDeleteFile
projectClose(TRUE)

Log("test_portal_endtoend OK")
