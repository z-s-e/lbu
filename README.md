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


## Design notes

 * the various *Status enums (such as io::ReadStatus) are ment to be a reasonable
   set of common errors to check for, excluding some more esoteric possible
   errors. To indicate that these enums are not exhaustive, functions like
   io::read() still return an int instead of the enum.
 * xmalloc and friends still return a raw pointer instead of unique_cpod, because
   all situations in the code that use it are managing the lifetime on their own.
   To me xmalloc appears to be primairly useful for creating such custom wrappers
   that don't need the automatic lifetime handling anyway, so unless this changes
   I'll leave it like this. Besides, this might be the more expected behavior of
   functions with that name, and adding a unique_cpod around it is trivial.
