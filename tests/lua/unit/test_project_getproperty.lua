package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- After load_rom, project is open; getProperty should return non-nil.
v = projectGetProperty(ePrjPropEcuSoftwaresize)
assert_true(type(v) == "string")
-- Software size for EDC17C46 fixture = 2097152 = "2097152"
assert_equal(v, "2097152")
Log("test_project_getproperty OK")
