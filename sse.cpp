#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef unsigned char uchar;

// We loop through one unsigned long at a time
typedef const unsigned long chunk_t;
const static int chunk_size = sizeof(chunk_t);

// Define what a bit counting function looks like
typedef long bit_counting_function(const uchar *buffer, size_t bufsize);

// The various implementations of bit counting functions
bit_counting_function count_bits_naive;
bit_counting_function count_bits_fast;
bit_counting_function count_bits_intrinsic;
// helper for count_bits_fast
bit_counting_function count_bits_sse;

// Timer
void time_bit_counting(bit_counting_function *func, int iters, const uchar *buffer, size_t bufsize);

long count_bits_naive(const uchar *buffer, size_t bufsize)
{
    long bitcount = 0;
    for(size_t byte = 0; byte < bufsize; byte++)
        for(int bit = 0; bit < 8; bit++)
            if (buffer[byte] & (1 << bit))
                bitcount++;
    return bitcount;
}

long count_bits_chunked(bit_counting_function *func, const uchar *buffer, size_t bufsize)
{
    const size_t num_chunks = bufsize / chunk_size;
    const size_t chunked_bufsize = num_chunks * chunk_size;
    const size_t leftover = bufsize - chunked_bufsize;
    const int leftover = bufsize - iterations * chunk_size;
    long total = func(buffer, bufsize -;
    for (size_t i = 0; i < iterations; i++)
    {
        chunk_t chunk = *reinterpret_cast<chunk_t *>(buffer);
        total += __builtin_popcountl(chunk);
        buffer += chunk_size;
    }
    total += count_bits_naive(buffer, leftover);
    return total;
}

long count_bits_intrinsic(const uchar *buffer, size_t bufsize)
{
    const size_t iterations = bufsize / chunk_size;
    const int leftover = bufsize - iterations * chunk_size;
    long total = 0;
    for (size_t i = 0; i < iterations; i++)
    {
        chunk_t chunk = *reinterpret_cast<chunk_t *>(buffer);
        total += __builtin_popcountl(chunk);
        buffer += chunk_size;
    }
    total += count_bits_naive(buffer, leftover);
    return total;
}

long count_bits_fast(const uchar *buffer, size_t bufsize)
{
    const size_t aligned_size = (bufsize / chunk_size) * chunk_size;
    long bitcount = count_bits_sse(buffer, bufsize);
    bitcount += count_bits_naive(buffer + aligned_size, bufsize - aligned_size);
    return bitcount;
}

inline long count_bits_sse(const uchar *buffer, size_t bufsize)
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

void time_bit_counting(bit_counting_function *func, int iters, const uchar *buffer, size_t bufsize)
{
    time_t start, duration;
    // How many iterations represent roughly 10% of the total.
    // Used because We print a dot after every 10%.
    int ten_percent = iters / 10;
    if (ten_percent < 10)
        //  Just print a dot after every one
        ten_percent = 1;

    start = time(NULL);
    for (int i = 0; i < iters; i++)
    {
        long num_bits = func(buffer, bufsize);
        if (i == 0)
            printf("%ld bits are set", num_bits);
        else if (! (i % ten_percent))
            printf(".");
        fflush(stdout);
    }
    duration = time(NULL) - start;
    printf("\n");
    printf("%f seconds per iteration\n", ((double)duration / iters));
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

    printf("Timing naive implementation\n");
    time_bit_counting(count_bits_naive, 10, buffer, bufsize);
    printf("Timing intrinsic implementation\n");
    time_bit_counting(count_bits_intrinsic, 100, buffer, bufsize);
    printf("Timing badass implementation\n");
    time_bit_counting(count_bits_fast, 1000, buffer, bufsize);

    return 0;
}
