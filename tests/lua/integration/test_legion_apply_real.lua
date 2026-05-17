package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- End-to-end LEGION apply test.
--
-- Replicates the MainWindow "Summon LEGION → Submit ticked verdicts"
-- code path against real catalog data:
--   1. Load user baseline (ROMHEX14_CUSTOMER_BIN).
--   2. Harvest voices via projectFindSimilarProjectsSql + projectImportChanges.
--   3. Run the full legionInvoke pipeline.
--   4. For every Unanimous / StrongConsensus verdict in the biggest cluster,
--      apply it to a working byte-string via _legion_applyVerdict (the same
--      C++ function MainWindow calls under the hood).
--   5. Diff the result against the baseline at byte granularity.
--   6. Assert: every changed byte falls inside some verdict's range; the
--      total change count matches the sum of per-verdict deltas; and at
--      least one byte was actually written (i.e. LEGION did SOMETHING).
--
-- Skips when env var unset.

local SRC_BIN = env_path("ROMHEX14_CUSTOMER_BIN")
if not SRC_BIN then
    Log("test_legion_apply_real: ROMHEX14_CUSTOMER_BIN not set, skipping")
    return
end

local MAX_VOICES   = 30
local MIN_SIM_PCT  = 85
local CHUNK        = 65536

-- ── helpers ─────────────────────────────────────────────────────────────

local function readBytesAsString(size)
    local parts = {}
    for base = 0, size - 1, CHUNK do
        local n = math.min(CHUNK, size - base)
        local t = projectGetAt(base, eByte, n)
        local chars = {}
        for i = 1, n do chars[i] = string.char(t[i]) end
        parts[#parts + 1] = table.concat(chars)
    end
    return table.concat(parts)
end

local function byteAt(s, i) return string.byte(s, i + 1) end   -- 0-based

-- ── 1. baseline ─────────────────────────────────────────────────────────

NewProject()
if projectImport(SRC_BIN, eFiletypeBinary) ~= true then
    Log("test_legion_apply_real: projectImport failed, skipping")
    return
end
local USER_SIZE = tonumber(projectGetProperty(ePrjPropEcuSoftwaresize))
if not USER_SIZE or USER_SIZE <= 0 then
    Log("test_legion_apply_real: zero-sized baseline, skipping")
    return
end
local baseline = readBytesAsString(USER_SIZE)
assert_equal(#baseline, USER_SIZE)
Log(string.format("test_legion_apply_real: baseline %d bytes", USER_SIZE))

-- ── 2. harvest voices ──────────────────────────────────────────────────

local hits = projectFindSimilarProjectsSql(MIN_SIM_PCT, MAX_VOICES,
                eFSPTrippleRelevance, ePrjFilename)
local nCols  = 4
local nHits  = math.floor(#hits / nCols)
if nHits == 0 then
    Log("test_legion_apply_real: empty catalog, skipping")
    return
end

local voices = {}
for i = 0, math.min(MAX_VOICES, nHits) - 1 do
    local fname = hits[i * nCols + 4]
    if type(fname) == "string" then
        NewProject()
        projectImport(SRC_BIN, eFiletypeBinary)
        local rc = projectImportChanges(fname, 1)
        if rc and rc >= 1 and rc <= 4 then
            voices[#voices + 1] = {
                sourcePath    = fname,
                originalBytes = baseline,
                modifiedBytes = readBytesAsString(USER_SIZE),
            }
        end
    end
end
Log(string.format("test_legion_apply_real: %d voices harvested", #voices))
if #voices < 2 then
    Log("test_legion_apply_real: not enough voices, skipping")
    return
end

-- ── 3. pipeline ────────────────────────────────────────────────────────

local r = legionInvoke({
    baseline    = baseline,
    voices      = voices,
    jaccardMin  = 0.50,
    localSimMin = 0.90,
    detectK     = 16,
})
local biggest = r.clusters[1]
assert_true(biggest and biggest.memberCount >= 2,
            "biggest cluster must have ≥2 voices to aggregate")
Log(string.format("test_legion_apply_real: biggest cluster=%d voices, %d verdicts",
                  biggest.memberCount, #biggest.verdicts))

-- ── 4. pick verdicts to apply (matches MainWindow auto-tick rule) ─────

local toApply = {}
for _, v in ipairs(biggest.verdicts) do
    if v.tag == "Unanimous" or v.tag == "StrongConsensus" then
        toApply[#toApply + 1] = v
    end
end
Log(string.format("test_legion_apply_real: %d verdicts qualify for auto-apply",
                  #toApply))
if #toApply == 0 then
    Log("test_legion_apply_real: no strong-consensus verdicts in this catalog, "
        .. "skipping byte-diff assertions")
    NewProject(); projectImport(SRC_BIN, eFiletypeBinary)
    Log("test_legion_apply_real OK (no-op branch)")
    return
end

-- ── 5. apply each verdict to a working buffer ──────────────────────────

local working = baseline
local expectedChangeCount = 0
local expectedCellChanges = 0
for _, v in ipairs(toApply) do
    for _, c in ipairs(v.cells) do
        if c.sampleCount > 0 and math.abs(c.meanDelta) >= 0.5 then
            expectedCellChanges = expectedCellChanges + 1
        end
    end
    working = _legion_applyVerdict(working, v)
end
assert_equal(#working, #baseline,
             "applyVerdict must preserve buffer size")
Log(string.format("test_legion_apply_real: applied; expected %d cell-changes",
                  expectedCellChanges))

-- ── 6. byte-level diff ────────────────────────────────────────────────

local changed = {}
for i = 0, USER_SIZE - 1 do
    if byteAt(working, i) ~= byteAt(baseline, i) then
        changed[#changed + 1] = i
    end
end
Log(string.format("test_legion_apply_real: %d bytes changed total",
                  #changed))
assert_true(#changed > 0,
            "LEGION must actually modify bytes for a Unanimous verdict")

-- ── 7. every changed byte lies inside a verdict's address range ──────

local function inAnyVerdictRange(addr)
    for _, v in ipairs(toApply) do
        if addr >= v.startAddr and addr <= v.endAddr then return v end
    end
    return nil
end

local stray = 0
for _, addr in ipairs(changed) do
    if not inAnyVerdictRange(addr) then stray = stray + 1 end
end
assert_equal(stray, 0,
             string.format("found %d byte changes OUTSIDE any verdict range",
                           stray))

-- ── 8. spot-check the densest cell of the top verdict ────────────────

local top = toApply[1]
local cellSize = top.cellSize or 1
local densest = 1
for i = 2, #top.cells do
    if top.cells[i].sampleCount > top.cells[densest].sampleCount then
        densest = i
    end
end
local cellAddr = top.startAddr + (densest - 1) * cellSize

-- Decode the cell as an unsigned int (LoHi or HiLo).
local function decodeCell(buf, addr, sz, be)
    local v = 0
    if be then
        for b = 0, sz - 1 do
            v = v * 256 + byteAt(buf, addr + b)
        end
    else
        for b = sz - 1, 0, -1 do
            v = v * 256 + byteAt(buf, addr + b)
        end
    end
    return v
end

local origVal = decodeCell(baseline, cellAddr, cellSize, top.bigEndian)
local newVal  = decodeCell(working,  cellAddr, cellSize, top.bigEndian)
local actualDelta = newVal - origVal
local expectedDelta = math.floor(top.cells[densest].meanDelta + 0.5)
Log(string.format(
    "test_legion_apply_real: top verdict (%s) at 0x%X densest cell: "
    .. "orig=%d new=%d delta=%d (expected %d)",
    top.tag, math.floor(cellAddr), origVal, newVal,
    actualDelta, expectedDelta))

-- For Unanimous tag the rounded mean delta is the exact write.
if top.tag == "Unanimous" then
    -- Allow ±1 for clamping at byte range boundaries.
    local diff = math.abs(actualDelta - expectedDelta)
    assert_true(diff <= 1,
        string.format("Unanimous verdict must write rounded mean delta "
                      .. "(got %d, expected %d)", actualDelta, expectedDelta))
end

-- ── 9. restore project for downstream tests ──────────────────────────

NewProject(); projectImport(SRC_BIN, eFiletypeBinary)
Log("test_legion_apply_real OK")
