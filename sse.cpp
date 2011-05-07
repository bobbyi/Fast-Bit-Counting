#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>




long count_bits_sse(void *buffer, size_t bufsize);
long count_bits_naive(void *buffer, size_t bufsize);

// helpers for count_bits_sse
inline long count_bits_aligned(void *buffer, size_t bufsize);
inline int count_bits_int_sse(unsigned int the_int);

inline long count_bits_aligned(void *buffer, size_t bufsize)
{
    const int iterations = bufsize / sizeof(unsigned);
    if (!iterations)
        return 0;
    int bitcount = 0;
    int total;
    int ecx;
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

long count_bits_sse(void *buffer, size_t bufsize)
{
    long bitcount = count_bits_aligned(buffer, bufsize);
    const size_t num_ints = bufsize / sizeof(int);
    const size_t num_chars= bufsize % sizeof(int);
    unsigned char *the_char = static_cast<unsigned char *>(buffer) + num_ints * sizeof(int);
    for (size_t which_char = 0; which_char < num_chars; which_char++)
    {
        bitcount += count_bits_int_sse(*the_char++);
    }
    return bitcount;
}

int count_bits_int_sse(unsigned int the_int)
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

long count_bits_naive(void *buffer, size_t bufsize)
{
    long bitcount = 0;
    unsigned char *bytes = static_cast<unsigned char*>(buffer);
    for(size_t byte = 0; byte < bufsize; byte++)
        for(int bit = 0; bit < 8; bit++)
            if (bytes[byte] & (1 << bit))
                bitcount++;
    return bitcount;
}

int main(int argc, char **argv)
{
    const char *filename = "/dev/urandom";
    const size_t bufsize = 100000000; 
    unsigned char *buffer = new unsigned char[bufsize];

    printf("Reading input...\n");
    FILE *infile = fopen(filename, "r");
    fread(buffer, 1, bufsize, infile);
    fclose(infile);
    printf("done reading input\n");

    int iters = 10;
    time_t start, duration;

    printf("Timing naive implementation\n");
    start = time(NULL);
    for (int i =0; i < iters; i++)
    {
        long num_bits = count_bits_naive(buffer, bufsize);
        if (i == 0)
            printf("%ld bits are set", num_bits);
        else
            printf(".");
        fflush(stdout);
    }
    printf("\n");
    duration = time(NULL) - start;
    printf("naive: %f seconds per iteration\n", ((double)duration / iters));

    printf("Timing badass implementation\n");
    start = time(NULL);
    iters = 100;
    for (int i =0; i < iters; i++)
    {
        long num_bits = count_bits_sse(buffer, bufsize);
        if (i == 0)
            printf("%ld bits are set", num_bits);
        else
            printf(".");
        fflush(stdout);
    }
    printf("\n");
    duration = time(NULL) - start;
    printf("SSE: %f seconds per iteration\n", ((double)duration / iters));
}
