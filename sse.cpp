#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef unsigned char uchar;
// We loop through one unsigned long at a time
typedef unsigned long chunk_t;
const static int chunk_size = sizeof(chunk_t);

long count_bits_fast(uchar *buffer, size_t bufsize);
long count_bits_naive(uchar *buffer, size_t bufsize);
// helper for count_bits_fast
inline long count_bits_sse(uchar *buffer, size_t bufsize);

inline long count_bits_intrinsic(uchar *buffer, size_t bufsize)
{
    const int iterations = bufsize / chunk_size;
    if (!iterations)
        return 0;
    long bitcount = 0;
    long total;
    for (size_t i =0; i < iterations; i ++)
        ;
    return total;
}

inline long count_bits_sse(uchar *buffer, size_t bufsize)
{
    size_t iterations = bufsize / chunk_size;
    if (!iterations)
        return 0;
    // This is a dummy output variable for the bitcount
    // calculated in each iteration.
    // Which is really a temporary register that we are clobbering.
    long bitcount;
    long total;

    __asm__ (
        // do {
        "1:"
        //     bitcount = popcnt(*buffer);
        "popcnt (%[buffer]), %[bitcount];"
        //     total += bitcount;
        "add %[bitcount], %[total];"
        //     buffer += chunk_size;
        "add %[chunk_size], %[buffer];"
        // } while(--total);
        "loop 1b;"

        // Output values
        :   [total]         "=&r"       (total), 
            [bitcount]      "=&r"       (bitcount),
            // ecx and buffer are really clobbered rather than output,
            // but gcc seems to like it better if we list them here.
            [ecx]           "=&c"       (iterations), 
            [buffer]        "=&r"       (buffer)

        // Input values
        :   [chunk_size]    "i"         (chunk_size), 
                            "[buffer]"  (buffer), 
                            "[ecx]"     (iterations), 
                            "[total]"   (0)

        // Clobbered registers
        // We pretty much declared them all as outputs, so they don't
        // need to be listed again.
        :   "cc"
    );
    return total;
}

long count_bits_fast(uchar *buffer, size_t bufsize)
{
    const size_t aligned_size = (bufsize / chunk_size) * chunk_size;
    long bitcount = count_bits_sse(buffer, bufsize);
    bitcount += count_bits_naive(buffer + aligned_size, bufsize - aligned_size);
    return bitcount;
}

long count_bits_naive(uchar *buffer, size_t bufsize)
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
    size_t megs_of_data = 100; 
    if (argc > 1)
    {
        megs_of_data = atol(argv[1]);
    }
    printf("Using %d megs of data\n", megs_of_data);

    size_t bufsize = megs_of_data * 1024 * 1024;
    uchar *buffer = new unsigned char[bufsize];

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
    duration = time(NULL) - start;
    printf("\n");
    printf("naive: %f seconds per iteration\n", ((double)duration / iters));

    printf("Timing badass implementation\n");
    start = time(NULL);
    iters = 100000;
    for (int i =0; i < iters; i++)
    {
        long num_bits = count_bits_fast(buffer, bufsize);
        if (i == 0)
            printf("%ld bits are set", num_bits);
        else if (! (i % 1000))
            printf(".");
        fflush(stdout);
    }
    duration = time(NULL) - start;
    printf("\n");
    printf("SSE: %f seconds per iteration\n", ((double)duration / iters));
}
