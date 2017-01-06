# llvm-pass-skeleton

from http://www.jianshu.com/p/45edd0535aac

## install llvm

 # xcode-select --install
 # brew install llvm
 # brew link llvm

```
➜  ~ clang --version
clang version 3.9.1 (tags/RELEASE_391/final)
Target: x86_64-apple-darwin16.3.0
Thread model: posix
InstalledDir: /usr/local/bin
➜  ~
```

## test llvm
A completely useless LLVM pass.

Build:

    $ mkdir build
    $ cd build
    $ cmake ..
    $ make
    $ cd ..

Run:

    $ clang -Xclang -load -Xclang build/skeleton/libSkeletonPass.* test.c

```
➜  0 git:(master) ✗ clang -Xclang -load -Xclang build/skeleton/libSkeletonPass.* test.c
test.c:4:5: warning: implicitly declaring library function 'printf' with type
      'int (const char *, ...)' [-Wimplicit-function-declaration]
    printf("test");
    ^
test.c:4:5: note: include the header <stdio.h> or explicitly provide a
      declaration for 'printf'
I saw a function called main!
1 warning generated.
➜  0 git:(master) ✗
```
