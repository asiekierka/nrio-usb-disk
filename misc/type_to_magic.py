#!/usr/bin/python

import sys

def type_to_magic(m):
    return '0x%02X%02X%02X%02X' % (ord(m[3]), ord(m[2]), ord(m[1]), ord(m[0]))

print(type_to_magic(sys.argv[1]))
