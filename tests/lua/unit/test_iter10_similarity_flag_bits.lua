package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Iter 10.8: similarity flags moved to high bits so they no longer collide
-- with property IDs (ePrjPropClientName=1 etc.) in the trailing column list.
assert_equal(eFSPAllowPropertyMatches, 0x10000)
assert_equal(eFSPTrippleRelevance,     0x20000)

-- Passing ePrjPropClientName (=1) as a column ID must not be mistaken for
-- a flag value.  We can't validate the actual SQL query without a populated
-- index, so we only check the call returns a table without error.
local r = projectFindSimilarProjectsSql(80, 5, ePrjPropClientName, ePrjPropClientNumber)
assert_true(type(r) == "table")

Log("test_iter10_similarity_flag_bits OK")
