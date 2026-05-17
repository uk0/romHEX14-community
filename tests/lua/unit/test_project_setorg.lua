package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- projectSetOrg(start, end) restores bytes in [start, end) from originalData.
-- Iter 11.4: pre-Iter-11 assertion was tautological (the OR chain made it
-- always true).  Now we capture the original, mutate to a distinct value,
-- and verify setOrg restored exactly that captured original.
local orig0 = projectGetAt(0x300, eByte)
local orig1 = projectGetAt(0x301, eByte)

-- Pick a mutation value that differs from each original.
local mut0 = (orig0 == 0xAB) and 0xCD or 0xAB
local mut1 = (orig1 == 0xCD) and 0xAB or 0xCD
projectSetAt(0x300, mut0, eByte)
projectSetAt(0x301, mut1, eByte)
-- Sanity: writes took effect.
assert_equal(projectGetAt(0x300, eByte), mut0, "byte0 mutation")
assert_equal(projectGetAt(0x301, eByte), mut1, "byte1 mutation")

-- Restore via setOrg and verify each byte matches the original snapshot.
projectSetOrg(0x300, 0x302)
assert_equal(projectGetAt(0x300, eByte), orig0, "byte0 restored to orig")
assert_equal(projectGetAt(0x301, eByte), orig1, "byte1 restored to orig")
Log("test_project_setorg OK")
