import os
import sys
import random
import string
import subprocess
from time import sleep

#
# Tester for procprog
# Example: make && ./procprog python3 self-test.py $(tput cols)
#

try:
    termWidth = int(sys.argv[1])
except:
    sys.exit("Add $(tput cols) to your command")

extented_ascii = [chr(i) for i in range(256)]


def fullWidth():
    for i in range(1,10):
        for _ in range(0,10):
            print(''.join(random.choices(string.ascii_lowercase, k=termWidth * i)), flush=True)
            sleep(0.01)
    for i in range(1,10):
        for _ in range(0,5):
            print(''.join(random.choices(string.ascii_lowercase, k=(termWidth * i) + 1)), flush=True)
            sleep(0.01)

    print('\n')

def simple():
    for i in range(1,500):
        for _ in range(0,10):
            print(''.join(random.choices(string.ascii_lowercase, k=i)), flush=True)
            sleep(0.001)

    for i in range(1,500):
        for _ in range(0,10):
            print(''.join(random.choices(string.whitespace, k=i)), flush=True)
            sleep(0.001)

    for i in range(1,500):
        for _ in range(0,10):
            print(''.join(random.choices(string.printable, k=i)), flush=True)
            sleep(0.001)

    for i in range(1,500):
        for _ in range(0,10):
            print(''.join(random.choices(extented_ascii, k=i)), flush=True)
            sleep(0.001)

    print('\n')

def diskBench():
    subprocess.run(["sudo", "hdparm", "-tT", "/dev/sda"])


if __name__ == '__main__':
    fullWidth()
    simple()

