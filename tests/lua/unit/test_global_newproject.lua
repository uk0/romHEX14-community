package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Iter 8 v2: only verify that NewProject is bound.  Actually invoking it
-- would put an empty project on the MDI stack and (depending on Qt's
-- subwindow-show heuristics) may make it active, starving subsequent
-- fixture-bound tests of the loaded ROM data.  The full NewProject + close
-- flow is exercised by test_zz_portal_endtoend.
assert_true(type(NewProject) == "function")
Log("test_global_newproject OK")
