# mstrcmp
`strcmp()` written in ARM Cortex-M Assembly. Designed for Cortex-M4 and M0+ cores. Probably works on other ARMv6-M and ARMv7-M architectures.

### Status 
Does everything I need it to. Will fix bugs as I come across them.

### How to use this in an embedded project
1. Copy `mstrcmp.s` from the common/src folder into your project
2. Add a function declaration for `strcmp_()` somewhere in your project. The function declaration should look something like `extern int strcmp_(const char *str1, const char *str2);`.
3. Use `strcmp_()` just like you would the normal `strcmp()` function.
4. Done!

### How to measure the size of this project
1. copy these files into an embedded project.
2. compile the project.
3. measure the size of the `mstrcmp.o` object file using `size -A`.
4. the total size is found from summing all relevant sections together.


### Details

Compiles down to 132 bytes on `arm-none-eabi-gcc`. mstrcmp.s is about 80 lines of code (per David A. Wheeler's `SLOCCount`).

In my testing, `strcmp_()` is almost always faster (or the same) versus the standard `strcmp()` used on the Cortex-M0+ core. It is slower than the `strcmp()` used on the Cortex-M4. Unlike the library functions, which only promise a return value of positive, negative, or 0, my implemention always returns the difference between bytes at the point where the strings differed.

This repo is designed to run on an STM32WLxx microcontroller. It uses the cycle counter of the Cortex-M4 microcontroller to profile the functions during testing.
Note: the environment this repo was developed in is only important to know if you want to test the `strcmp_()` function like I did. If you just want to use it, follow the instructions in [How to use this in an embedded project](#how-to-use-this-in-an-embedded-project).

I also included testing using Greatest from https://github.com/silentbicycle/greatest. There's a number of tests that compare `strcmp_()` against the standard `strcmp()`.
