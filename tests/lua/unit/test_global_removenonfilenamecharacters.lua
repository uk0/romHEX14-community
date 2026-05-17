package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Pass a string with all invalid chars and verify they're removed
result = RemoveNonFilenameCharacters("a/b\\c:d*?e<f>g|h")
-- Expect just letters left
assert_equal(result, "abcdefgh")
assert_equal(RemoveNonFilenameCharacters("clean.txt"), "clean.txt")
Log("test_global_removenonfilenamecharacters OK")
