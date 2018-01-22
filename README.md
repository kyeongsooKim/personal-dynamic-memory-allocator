# Personal Dynamic Memory Allocator
### Kyeongsoo Kim, September 2017 ~ October 2017


# Introduction
The goal of this project is to understand The inner workings of a dynamic memory allocator, Memory padding and alignment, and linked lists data structure, memory hierarchy, and unit testing. I made my personal dynamic memory allocator. It's a segregated free list allocator for the x86-64 architecture
that manages up to 4 pages of memory at a time with the following features:
- Four explicit free lists
- A first-fit placement policy
- Immediate coalescing on free with blocks at higher memory addresses
    - Note: coalescing with lower memory addresses happens only when requesting more
memory from `sf_sbrk`.
- Block splitting without creating splinters.

Free blocks will be inserted into the proper free list in **last in first out
(LIFO) order**.

This personal dynamic memory allocator provides  **malloc**, **realloc**, and
**free** functions.

Plus, to check to functionality of this program, I made several criterion unit tests.


# Caution

- This allocator works with 4 pages of memory (1 page = 4096 bytes). Allocations
over 4 pages is not allowed.
- alignment and padding works correctly in the system I used when I did the project(64-bit Linux Mint).

# Unit Test.

For this assignment, we will use Criterion to test your allocator. We have
provided a basic set of test cases and you will have to write your own as well.

You will use the Criterion framework alongside the provided helper functions to
To ensure my allocator works exactly, I made unit testing file using the C criterion framework in the `tests/sfmm_tests.c` file.
## Compiling and Running Tests

When you compile your program with `make`, a `sfmm_tests` executable will be
created in the `bin` folder alongside the `main` executable. This can be run
with `bin/sfmm_tests`. To obtain more information about each test run, you can
use the verbose print option: `bin/sfmm_tests --verbose=0`.
