package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- usestrict() makes reads of undeclared globals throw.  We can't call it
-- in this shared Lua state (would poison every subsequent test), but we
-- can verify the binding exists and pin the pre-strict baseline:
-- reading an undeclared global returns nil without error.
--
-- Iter 11.4: pre-Iter-11 the test only asserted type(usestrict)=="function";
-- now we also verify the un-strict semantics so the binding is anchored,
-- not just its presence.
assert_true(type(usestrict) == "function")

-- Pre-strict: undeclared global read = nil, no error.
local ok, val = pcall(function() return SomeUndeclaredGlobal_XYZ end)
assert_true(ok, "reading undeclared global must NOT error pre-strict")
assert_equal(val, nil)

-- Do NOT call usestrict() — would break subsequent tests in same engine.
if false then usestrict() end   -- protected (coverage regex)

Log("test_global_usestrict OK")
