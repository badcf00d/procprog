# procprog
A program for monitoring output in a less verbose way

### To use include-what-you-used:
    Symbolic link the version of clang iwyu was built for to the one on your system,
    e.g. if iwyu wants clang-9 (see include-what-you-use -print-resource-dir), and you have clang-10 installed
```
sudo ln -s /usr/lib/clang/10 /usr/lib/clang/9.0.1
```
    Then run `make -k iwyu`
