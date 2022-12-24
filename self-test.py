#!/usr/bin/python3
import os
import sys
import random
import string
import subprocess
import re
import math
from time import sleep

#
# Tester for procprog
# Example: make debug && ./procprog ./self-test.py $(tput cols)
#

print("hello")

def cursorPos():
    _ = ""
    sys.stdout.write("\x1b[6n")
    sys.stdout.flush()
    while not (_ := _ + sys.stdin.read(1)).endswith('R'):
        True
    res = re.match(r".*\[(?P<y>\d*);(?P<x>\d*)R", _)

    if(res):
        return (int(res.group("x")), int(res.group("y")))
    return (-1, -1)

try:
    term_width = int(sys.argv[1])
except:
    term_width = 80
    print("\033[33mWarning Add $(tput cols) to your command, assuming 80\033[0m", flush=True)
    sleep(3)

extented_ascii = [chr(i) for i in range(256)]
dec_private_modes = [str(i) for i in range(100)]
dec_private_modes.extend([str(i) for i in range(1000, 1062)])
dec_private_modes.append('2004')
starting_x, starting_y = cursorPos()


def prepare_check():
    global starting_x
    global starting_y
    starting_x, starting_y = cursorPos()

def check_cursor_pos(num_chars, test_string):
    correct_x = starting_x
    correct_y = starting_y #+ (math.ceil(num_chars / term_width) - 1)
    actual_x, actual_y = cursorPos()

    if (actual_x != correct_x) or (actual_y != correct_y):
        print(f"\033[33mCursor in wrong position ({actual_x}, {actual_y}) ({num_chars}, {term_width}, {starting_y}) \
            should be ({correct_x}, {correct_y}). Input: {test_string})\033[0m", flush=True)
        sys.exit(1)


def csi_commands():
    print('Starting csi_commands test 1', flush=True)
    sleep(0.5)
    for _ in range(0,500):
        command = 'test\033[' + ''.join(random.choices(['-1', '', '0', '1', '2', '4', '5', '6'], k=1))
        command += ''.join(random.choices(string.ascii_letters, k=1))
        print(command, flush=True)

    print('Starting csi_commands test 2', flush=True)
    sleep(0.5)
    for _ in range(0,500):
        command = 'test\033[' + ''.join(random.choices(['-1', '', '0', '1', '2', '4', '5', '6'], k=1))
        command += ';' + ''.join(random.choices(['-1', '', '0', '1', '2', '4', '5', '6'], k=1))
        command += ''.join(random.choices(string.ascii_letters, k=1))
        print(command, flush=True)

    print('Starting csi_commands test 3', flush=True)
    sleep(0.5)
    for _ in range(0,500):
        command = '\033[' + str(random.randint(0,110)) + 'mtest'
        print(command, flush=True)

    print('\033[0mStarting csi_commands test 4', flush=True)
    sleep(0.5)
    for _ in range(0,500):
        command = 'test\033[?' + ''.join(random.choices(dec_private_modes, k=1))
        command += ''.join(random.choices(['h', 'l'], k=1))
        print(command, flush=True)



def full_width():
    print('Starting full_width test 1', flush=True)
    sleep(0.5)
    for i in range(1,10):
        for _ in range(0,10):
            test_string = ''.join(random.choices(string.ascii_lowercase, k=term_width * i))
            print(test_string, flush=True)

    print('Starting full_width test 2', flush=True)
    sleep(0.5)
    for i in range(1,10):
        for _ in range(0,10):
            test_string = ''.join(random.choices(string.ascii_lowercase, k=(term_width * i) + 1))
            print(test_string, flush=True)

    print('Starting full_width test 3', flush=True)
    sleep(0.5)
    for i in range(1,256):
        print('a' * term_width, flush=True, end='')
        test_string = ''.join(chr(i))
        print(test_string, flush=True)

    print('Starting full_width test 4', flush=True)
    sleep(0.5)
    for i in range(1,256):
        print('a' * (term_width + 1), flush=True, end='')
        test_string = ''.join(chr(i))
        print(test_string, flush=True)

    print('Starting full_width test 5', flush=True)
    sleep(0.5)
    for i in range(1,256):
        print('a' * term_width, flush=True, end='')
        test_string = ''.join(chr(i))
        test_string += 'a\nb'
        print(test_string, flush=True)

    print('Starting full_width test 6', flush=True)
    sleep(0.5)
    for i in range(1,256):
        print('a' * (term_width + 1), flush=True, end='')
        test_string = ''.join(chr(i))
        test_string += 'a\nb'
        print(test_string, flush=True)

    print('\n')


def simple():
    print('Starting simple test 1', flush=True)
    sleep(0.5)
    for i in range(1,10):
        print(str(i), flush=True)
        sleep(0.1)

    print('Starting simple test 2', flush=True)
    sleep(0.5)
    for i in range(1,500):
        for _ in range(0,10):
            test_string = ''.join(random.choices(string.ascii_lowercase, k=i))
            print(test_string, flush=True)

    print('Starting simple test 3', flush=True)
    sleep(0.5)
    for i in range(1,500):
        for _ in range(0,10):
            test_string = ''.join(random.choices(string.whitespace, k=i))
            print(test_string, flush=True)

    print('Starting simple test 4', flush=True)
    sleep(0.5)
    for i in range(1,500):
        for _ in range(0,10):
            test_string = ''.join(random.choices(string.printable, k=i))
            print(test_string, flush=True)

    print('Starting simple test 5', flush=True)
    sleep(0.5)
    #prepare_check()
    for i in range(1,500):
        for _ in range(0,10):
            test_string = ''.join(random.choices(extented_ascii, k=i))
            print(test_string, flush=True)
            #check_cursor_pos(i, test_string)

    print('\033[0m\n')

def diskBench():
    subprocess.run(["sudo", "hdparm", "-tT", "/dev/sda"])


if __name__ == '__main__':
    print(f"Starting ({starting_x}, {starting_y})", flush=True)
    sleep(1)
    simple()
    full_width()
    csi_commands()


