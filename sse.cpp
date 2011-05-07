#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>



int count_bits_loop(void *buffer, size_t bufsize);
int count_bits_aligned(void *buffer, size_t bufsize);
int count_bits_naive(void *buffer, size_t bufsize);
inline int count_bits_sse(unsigned int the_int);

// assumes 0 == bufsize % sizeof(unsigned)
int count_bits_aligned(void *buffer, size_t bufsize)
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

int count_bits_loop(void *buffer, size_t bufsize)
{
    int bitcount = count_bits_aligned(buffer, bufsize);
    const size_t num_ints = bufsize / sizeof(int);
    const size_t num_chars= bufsize % sizeof(int);
    unsigned char *the_char = static_cast<unsigned char *>(buffer) + num_ints * sizeof(int);
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

int count_bits_naive(void *buffer, size_t bufsize)
{
    int bitcount = 0;
    unsigned char *bytes = static_cast<unsigned char*>(buffer);
    for(int byte = 0; byte < bufsize; byte++)
        for(int bit = 0; bit < 8; bit++)
            if (bytes[byte] & (1 << bit))
                bitcount++;
    return bitcount;
}

int main(int argc, char **argv)
{
    char *filename = argc > 1 ? argv[1] : argv[0];
    struct stat filestatus;
    stat( filename, &filestatus );

    const size_t bufsize = filestatus.st_size;
    unsigned char *buffer = new unsigned char[bufsize];

    FILE *infile = fopen(filename, "r");
    fread(buffer, 1, bufsize, infile);
    fclose(infile);

    for(int i = 0; i < 100; i++) {
         count_bits_loop(buffer, bufsize);
         //count_bits_naive(buffer, bufsize);
    }
    return 0;
}
