# Linux base utilities (lbu)

This library is an experimental utility library with the goal of providing
low level high performance application building blocks for Linux with a modern
C++14 interface and minimum abstraction overhead.

Goals of this library are:

 * be a testing ground for researching alternate (and hopefully better) API
   designs
 * wrap the Linux (syscall) interface in a convenient and safe C++ API
 * provide adapter between Linux resp. general C interfaces and standard
   library types
 * generally use the standard library, but also provide essential tools that
   are missing or sub-optimal in std
 * performance & minimalism
 * support both gcc and clang


## Status
 
This library is in a very early stage; lots of functionality, documentation
and tests are still missing.
 
**The API is not stable yet!**