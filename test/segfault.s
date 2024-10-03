# Is supposed to cause a segfault
lui x3, 0x10
lui x4, 0x50
ld x11, 8(x4)
ld x2, 4(x0)
ld x10, 0(x4)
ld x11, 8(x4)
