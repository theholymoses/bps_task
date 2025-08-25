#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

void
dlib2_sym1 (void)
{
    puts ("dlib2_sym1 called");
}

void
dlib2_sym2 (void)
{
    puts ("dlib2_sym2 called");
}

void
dlib2_sym3 (void)
{
    puts ("dlib2_sym3 called, counting from 0 to INT_MAX");

    volatile int i = 0;
    while (i < INT_MAX)
        ++i;

    puts ("done");
}

void
dlib_common_sym (void)
{
    puts ("dlib_common_sym(dlib2) called");
    exit (2);
}
