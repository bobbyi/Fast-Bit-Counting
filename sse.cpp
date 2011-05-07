#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

long count_bits_fast(unsigned char *buffer, size_t bufsize);
long count_bits_naive(unsigned char *buffer, size_t bufsize);
// helper for count_bits_fast
inline long count_bits_sse(unsigned char *buffer, size_t bufsize);

inline long count_bits_sse(unsigned char *buffer, size_t bufsize)
{
    const int iterations = bufsize / sizeof(unsigned long);
    if (!iterations)
        return 0;
    long bitcount = 0;
    long total;
    int ecx;
    __asm__ (
        "1:"
        "popcnt (%1), %0;"
        "add %0, %3;"
        "add %5, %1;"
        "loop 1b;"
        : "=&r" (bitcount), "=&r" (buffer), "=&c" (ecx), "=&r" (total)
        : "1" (buffer), "i" (sizeof(unsigned long)), "2" (iterations), "3" (total)
        : "cc"
    );
    return total;
}

long count_bits_fast(unsigned char *buffer, size_t bufsize)
{
    const size_t aligned_size = (bufsize / sizeof(unsigned long)) * sizeof(unsigned long);
    long bitcount = count_bits_sse(buffer, bufsize);
    bitcount += count_bits_naive(buffer + aligned_size, bufsize - aligned_size);
    return bitcount;
}

long count_bits_naive(unsigned char *buffer, size_t bufsize)
{
    long bitcount = 0;
    for(size_t byte = 0; byte < bufsize; byte++)
        for(int bit = 0; bit < 8; bit++)
            if (buffer[byte] & (1 << bit))
                bitcount++;
    return bitcount;
}

int main(int argc, char **argv)
{
    const char *filename = "/dev/urandom";
    size_t bufsize = 2000000000; 
    unsigned char *buffer = new unsigned char[bufsize];

    printf("Reading input...\n");
    FILE *infile = fopen(filename, "r");
    fread(buffer, 1, bufsize, infile);
    fclose(infile);
    printf("done reading input\n");

    // Let's make the data unaligned so it's even harder for SSE
    // who sometimes cares about such things
    buffer += 1;
    bufsize -= 1;

    int iters;
    time_t start, duration;

    printf("Timing naive implementation\n");
    start = time(NULL);
    iters = 3;
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
    iters = 10000;
    for (int i =0; i < iters; i++)
    {
        long num_bits = count_bits_fast(buffer, bufsize);
        if (i == 0)
            printf("%ld bits are set", num_bits);
        else if (! (i % 100))
            printf(".");
        fflush(stdout);
    }
    printf("\n");
    duration = time(NULL) - start;
    printf("SSE: %f seconds per iteration\n", ((double)duration / iters));
}
