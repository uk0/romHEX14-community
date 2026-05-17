package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Iter 10.7: window*MapProperties accept the third arg as a map selector:
--   nil / 0      → last map in list
--   1 (or true)  → m_luaLastCreatedMap (the one projectAddMap returned)
--   uint > 1     → find by start address
--   string       → find by MapId / Name

-- We work on a throwaway NewProject so we don't pollute the fixture.
NewProject()
projectAddMap()
windowSetMapProperties("Name", "AlphaMap", true)   -- target last-new
windowSetMapProperties("Feldwerte.StartAddr", 0x100, true)

projectAddMap()
windowSetMapProperties("Name", "BetaMap", true)
windowSetMapProperties("Feldwerte.StartAddr", 0x200, true)

-- Without selector → last map (Beta).
local lastName = windowGetMapProperties("Name")
assert_equal(lastName, "BetaMap", "no-selector → last map")

-- Select by address.
local atAlpha = windowGetMapProperties("Name", 0x100)
assert_equal(atAlpha, "AlphaMap", "address selector finds Alpha")
local atBeta = windowGetMapProperties("Name", 0x200)
assert_equal(atBeta, "BetaMap", "address selector finds Beta")

-- Select by MapId / Name string.
local byName = windowGetMapProperties("Feldwerte.StartAddr", "AlphaMap")
assert_equal(tonumber(byName), 0x100, "name selector finds Alpha addr")

-- Write via address selector.
windowSetMapProperties("Kommentar", "via-addr", 0x100)
local cmt = windowGetMapProperties("Kommentar", 0x100)
assert_equal(cmt, "via-addr", "write via address selector landed on Alpha")

-- Write via MapId-string selector.
windowSetMapProperties("Kommentar", "via-name", "BetaMap")
local cmt2 = windowGetMapProperties("Kommentar", "BetaMap")
assert_equal(cmt2, "via-name", "write via name selector landed on Beta")

projectClose()
Log("test_iter10_map_selector_overloads OK")
