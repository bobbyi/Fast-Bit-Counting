#include <stdio.h>

int main(int argv, char **argc)
{
    unsigned int my;
    for(int bit = 0; bit < sizeof(my) * 8; bit++)
    {
        if (bit == 2) continue;
        if (bit == 12) continue;
        unsigned int theval = (1 << bit);
        my |= theval;
    }

    int bitcount = 0;
    for(int bit = 0; bit < sizeof(my) * 8; bit++)
    {
        int bit_is_set;
        __asm__ ( 
            "bt %2, (%1);"
            "setc %b0;"

            : "=g" (bit_is_set)
            : "r" (&my), "r" (bit)
            : "cc"
        );
        bitcount += bit_is_set;
    }
    printf("%d\n", bitcount);

    return 0;
}
