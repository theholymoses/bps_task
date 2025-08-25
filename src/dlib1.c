#include <stdio.h>
#include <stdlib.h>

void
dlib1_sym1 (void)
{
    puts ("dlib1_sym1 called");
}

void
dlib1_sym2 (void)
{
    puts ("dlib1_sym2 called");
}

void
dlib1_sym3 (void)
{
    puts ("dlib1_sym3 called");
}

void
dlib_common_sym (void)
{
    puts ("dlib_common_sym(dlib1) called");
    exit (1);
}

