#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>

const int naive_iters = 10;
const int fast_iters = 100;

typedef unsigned char uchar;

// A bit counting function is a function that takes a buffer
// and returns a count of the number of bits set.
typedef long bit_counting_function(const uchar *buffer, size_t bufsize);

// The various implementations of bit counting functions
bit_counting_function count_bits_naive;
bit_counting_function count_bits_fast;
bit_counting_function count_bits_intrinsic;

// Utility function for count_bits_fast
inline long count_bits_fast_chunked(const uchar *buffer, size_t bufsize);

// The SEE implementations work in long-sized chunks
typedef const unsigned long chunk_t;
const static int chunk_size = sizeof(chunk_t);


// Iterate through the buffer one bit at a time
long count_bits_naive(const uchar *buffer, size_t bufsize)
{
    long bitcount = 0;
    for(size_t byte = 0; byte < bufsize; byte++)
        for(int bit = 0; bit < 8; bit++)
            if (buffer[byte] & (1 << bit))
                bitcount++;
    return bitcount;
}

int num_threads()
{
    int n_threads;
#pragma omp parallel shared(n_threads)
    {
#pragma omp master
        {
            n_threads = omp_get_num_threads();
        }
    }
    return n_threads > 0 ? n_threads : -1;
}

long count_bits_fast(const uchar *buffer, size_t bufsize)
{
    const int num_cores = num_threads();
    const size_t num_chunks = bufsize / chunk_size;
    const size_t chunks_per_core = num_chunks / num_cores;
    const size_t bufsize_per_core = chunks_per_core * chunk_size;
    const size_t chunked_bufsize = num_cores * bufsize_per_core;
    const size_t leftover = bufsize - chunked_bufsize;

    long total = 0;

#pragma omp parallel for reduction (+:total)
    for (int core = 0; core < num_cores; core++)
    {
        const uchar *mybuffer = buffer + core * bufsize_per_core;
        const long num_bits = count_bits_fast_chunked(mybuffer, bufsize_per_core);
        total += num_bits;
    }

    total += count_bits_naive(buffer + chunked_bufsize, leftover);

    return total;
}

// Count the bits in a buffer that is divisible by chunk_size using SSE instrinsics
long count_bits_intrinsic(const uchar *buffer, size_t bufsize)
{
    const size_t iterations = bufsize / chunk_size;
    const int leftover = bufsize - iterations * chunk_size;
    long total = 0;

#pragma omp parallel for reduction (+:total)
    for (size_t i = 0; i < iterations; i++)
    {
        chunk_t chunk = *reinterpret_cast<chunk_t *>(buffer + i * chunk_size);
        total += __builtin_popcountl(chunk);
    }

    total += count_bits_naive(buffer + iterations * chunk_size, leftover);
    return total;
}

// Count the bits in a buffer that is divisible by chunk_size using SSE ASM
inline long count_bits_fast_chunked(const uchar *buffer, size_t bufsize)
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

// Time how fast a bit counting function is
void time_bit_counting(const char *description, bit_counting_function *func, const uchar *buffer, size_t bufsize, int iters = fast_iters)
{
    time_t start, duration;
    // How many iterations represent roughly 10% of the total.
    // Used because We print a dot after every 10%.
    int ten_percent = iters / 10;
    if (ten_percent < 10)
        //  Just print a dot after every one
        ten_percent = 1;

    printf("Timing %s ", description);
    start = time(NULL);
    for (int i = 0; i < iters; i++)
    {
        long num_bits = func(buffer, bufsize);
        if (i == 0)
            printf("(%ld bits are set) ", num_bits);
        else if (! (i % ten_percent))
            printf(".");
    }
    duration = time(NULL) - start;
    printf("\n");
    printf("%f seconds per iteration\n", ((double)duration / iters));
}

int main(int argc, char **argv)
{
    // Unbuffered stdout
    setvbuf(stdout,NULL,_IONBF,0);

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
    uchar *original_buffer = buffer;
    buffer += 1;
    bufsize -= 1;

    time_bit_counting("naive implementation",
                      count_bits_naive, buffer, bufsize, naive_iters);
    time_bit_counting("intrinsic implementation (parallel)",
                      count_bits_intrinsic, buffer, bufsize);
    time_bit_counting("asm implementation (parallel)",
                      count_bits_fast, buffer, bufsize);

    // Turn off parallelism
    omp_set_num_threads(1);

    time_bit_counting("intrinsic implementation (serial)",
                      count_bits_intrinsic, buffer, bufsize);
    time_bit_counting("asm implementation (serial)",
                      count_bits_fast, buffer, bufsize);

    delete [] original_buffer;
    return 0;
}
