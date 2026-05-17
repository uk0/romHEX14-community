package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Stress: binaryor/binaryxor/binaryand with multiple operands, varying widths,
-- and edge masks. Variadic API must reduce across all args.

-- binaryor
assert_equal(binaryor(0x01, 0x02, 0x04, 0x08), 0x0F, "or 4-arg byte")
assert_equal(binaryor(0xFF00, 0x00FF), 0xFFFF, "or 16-bit mask")
assert_equal(binaryor(0, 0, 0), 0, "or all zero")
assert_equal(binaryor(0xAAAA, 0x5555), 0xFFFF, "or alternating bits")

-- binaryand
assert_equal(binaryand(0xFF, 0x0F), 0x0F, "and mask low nibble")
assert_equal(binaryand(0xAAAA, 0x5555), 0, "and alternating bits = 0")
assert_equal(binaryand(0xDEADBEEF, 0xFFFF), 0xBEEF, "and 32-bit low word")
assert_equal(binaryand(0xFFFF, 0xFFFF, 0xFFFF), 0xFFFF, "and 3-arg identity")
assert_equal(binaryand(0xFF, 0x80, 0x80), 0x80, "and 3-arg narrowing")

-- binaryxor
assert_equal(binaryxor(0xFF, 0xFF), 0, "xor self = 0")
assert_equal(binaryxor(0x12, 0x34), 0x26, "xor byte pair")
assert_equal(binaryxor(0xFFFF, 0x0F0F), 0xF0F0, "xor 16-bit invert nibbles")
assert_equal(binaryxor(0x01, 0x02, 0x04), 0x07, "xor 3-arg")
assert_equal(binaryxor(0xAA, 0xAA, 0xAA), 0xAA, "xor 3-arg odd self")

Log("test_stress_binary_ops OK")
