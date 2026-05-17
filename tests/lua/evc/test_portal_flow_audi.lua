-- Portal-flow integration test against a real customer .bin.
--
-- Mirrors what a typical tuner portal does (find similar projects,
-- import stage1 changes, export the tuned buffer):
--
--   1. Active project is a customer-supplied 2 MB ECU backup .bin.
--   2. projectFindSimilarProjectsSql against the local SimilarityIndex
--      DB (populated by WinOLS catalog indexing).
--   3. Pick a similar .ols (or .rx14proj) match.
--   4. projectImportChanges(srcFile, 1) — diff source version 1 vs.
--      source Original, apply to our active project.  Handles both
--      .rx14proj (via Project::open) and .ols (via OlsImporter).
--   5. Export the tuned buffer next to the source.
--
-- All paths are read from environment so the repo test file stays
-- free of personal data.  Required env vars:
--   ROMHEX14_CUSTOMER_BIN — full path to a 2 MB ECU backup .bin
--   ROMHEX14_CUSTOMER_OUT — output .bin path (gets overwritten)

package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

local SRC_BIN = env_path("ROMHEX14_CUSTOMER_BIN")
local OUT_BIN = env_path("ROMHEX14_CUSTOMER_OUT")
local MIN_SIM = 95

if not SRC_BIN or not OUT_BIN then
    Log("test_portal_flow_audi skipped — set ROMHEX14_CUSTOMER_BIN + ROMHEX14_CUSTOMER_OUT to run")
    return
end

-- Re-import the customer backup into the active project (the runner has
-- the 2 MB fixture loaded; size matches, so projectImport replaces the
-- byte buffer in place).
local ok = projectImport(SRC_BIN, eFiletypeBinary)
assert_true(ok, "projectImport on customer .bin must succeed; err="..tostring(GetLastError(true)))
Log("loaded customer backup ("..projectGetProperty(ePrjPropEcuSoftwaresize).." bytes)")

-- §2.4.31 similarity search with triple-relevance.
local hits = projectFindSimilarProjectsSql(
    MIN_SIM, 20, eFSPTrippleRelevance, ePrjFilename)
assert_true(type(hits) == "table", "similarity must return a table")
local nCols = 4              -- 3 relevance cells + 1 column (filename)
local nHits = #hits / nCols
Log(string.format("similar projects >= %d%%: %d hits", MIN_SIM, math.floor(nHits)))
assert_true(nHits >= 1, "expected at least one similar project in the index")

-- Walk hits, try projectImportChanges on each until one works.
local pickedFile, rc, err = nil, 0, ""
for i = 0, math.floor(nHits) - 1 do
    local base    = i * nCols
    local overall = hits[base + 1]
    local fname   = hits[base + 4]
    if type(fname) == "string" then
        local lower = fname:lower()
        if lower:sub(-4) == ".ols" or lower:sub(-9) == ".rx14proj" then
            local tryRc = projectImportChanges(fname, 1)
            if tryRc >= 1 and tryRc <= 4 then
                pickedFile, rc = fname, tryRc
                Log(string.format("  hit %d (overall=%s) imported OK  %s",
                                  i + 1, tostring(overall), fname))
                break
            else
                local thisErr = GetLastError(true)
                Log(string.format("  hit %d skip  rc=%s err=%s  %s",
                                  i + 1, tostring(tryRc), thisErr, fname))
                err = thisErr
            end
        end
    end
end
assert_true(pickedFile ~= nil,
            "no similar project's stage1 had compatible size; lastErr="..err)
assert_true(rc >= 1 and rc <= 4,
            "projectImportChanges must return a valid status (got "..tostring(rc)..")")

-- Verify something actually changed.
local diff = projectCountDifferentBytes(true)
Log(string.format("modified bytes vs. original: %d", diff))
assert_true(diff > 0, "import must have changed at least one byte")

-- Export the tuned buffer.
os.remove(OUT_BIN)
local okExp = projectExport(OUT_BIN, eFiletypeBinary)
assert_equal(okExp, true, "export must succeed; err="..tostring(GetLastError(true)))
assert_true(DoesFileExist(OUT_BIN), "output .bin must be on disk")

Log("test_portal_flow_audi OK  diffBytes="..diff.."  out="..OUT_BIN)
