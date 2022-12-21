#!/usr/bin/python3
import sys
from time import sleep

log = open(sys.argv[1], "rb")

for i, line in enumerate(log.readlines()):
    try:
        if not line.decode('cp437').startswith('stdin'):
            byte = line.decode('cp437')[7:8] # WARNING, you may need to change this
            print(byte, end='', flush=True)
    except:
        print(line)
        raise SystemExit
