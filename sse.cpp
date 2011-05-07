#include <stdio.h>
#include <string.h>

int count_bits_buffer(void *buffer, size_t bufsize);
inline int count_bits_sse(unsigned int the_int);
inline int count_bits(unsigned int the_int);

// assumes 0 == bufsize % sizeof(unsigned)
int count_bits_buffer(void *buffer, size_t bufsize)
{
    int bitcount = 0;
    int total = 0;
    int ecx;
    const int iterations = bufsize / sizeof(unsigned);
    if (!iterations)
        return 0;
    __asm__ (
        "1:"
        "popcnt (%1), %0;"
        "add %0, %7;"
        "add %5, %1;"
        "loop 1b;"
        : "=&r" (bitcount), "=&r" (buffer), "=&c" (ecx), "=&r" (total)
        : "1" (buffer), "i" (sizeof(unsigned)), "2" (iterations), "3" (total)
        : "cc"
    );
    return total;
}

int count_bits_buffer_loop(void *buffer, size_t bufsize)
{
    const size_t num_ints = bufsize / sizeof(int);
    const size_t num_chars= bufsize % sizeof(int);
    unsigned int *the_int = static_cast<unsigned int *>(buffer);
    int bitcount = 0;
    for (size_t which_int = 0; which_int < num_ints; which_int++)
    {
        bitcount += count_bits_sse(*the_int++);
    }
    unsigned char *the_char = reinterpret_cast<unsigned char *>(the_int);
    for (size_t which_char = 0; which_char < num_chars; which_char++)
    {
        bitcount += count_bits_sse(*the_char++);
    }
    return bitcount;
}

inline int count_bits_sse(unsigned int the_int)
{
    int bitcount = 0;
    __asm__ ( 
        "popcnt %1, %0;"

        : "=r" (bitcount)
        :  "r" (the_int)
        : "cc"
    );
    return bitcount;
}

inline int count_bits(unsigned int the_int)
{
    int bitcount = 0;
    for(int bit = 0; bit < sizeof(the_int) * 8; bit++)
    {
        int bit_is_set;
        __asm__ ( 
            "bt %1, (%2);"
            "setc %b0;"

            : "=g" (bit_is_set)
            :  "r" (bit), "r" (&the_int)
            : "cc"
        );
        bitcount += bit_is_set;
    }
    return bitcount;
}

int main(int argv, char **argc)
{
    unsigned int my = 0;
    for(int bit = 0; bit < sizeof(my) * 8; bit++)
    {
        if (bit == 2) continue;
        if (bit == 12) continue;
        unsigned int theval = (1 << bit);
        my |= theval;
    }

    printf("%d\n", count_bits_sse(my));

    unsigned int ints[3];
    ints[0] = my;
    ints[1] = 0;
    ints[2] = my;
    const size_t bufsize = sizeof(ints) + 2;
    unsigned char *buffer = new unsigned char[bufsize];
    memcpy(buffer, ints, sizeof(ints));
    buffer[bufsize - 1] = 1;
    buffer[bufsize - 2] = 3;
    printf("%d\n", count_bits_buffer(buffer, 2+sizeof(ints)));
    printf("%d\n", count_bits_buffer_loop(buffer, 2+sizeof(ints)));
    return 0;
}
