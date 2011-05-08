#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>

#include <iostream>
#include <fstream>

using namespace std;

typedef unsigned char uchar;

// A bit counting function is a function that takes a buffer
// and returns a count of the number of bits set.
typedef long bit_counting_function(const uchar *buffer, size_t bufsize);

// The various implementations of bit counting functions
bit_counting_function count_bits_naive; // Use simple C loop
bit_counting_function count_bits_fast; // Use SSE intrinsics
bit_counting_function count_bits_intrinsic; // Use inline ASM with SSE

// Utility functions for count_bits_fast
inline long count_bits_asm(const uchar *buffer, size_t bufsize);
int num_threads();

// The SEE implementations work in long-sized chunks
typedef const unsigned long chunk_t;
const static int chunk_size = sizeof(chunk_t);

// How may trials to use for timing the slow and fast implementations
const int naive_iters = 10;
const int fast_iters = 100;


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

// Count the bits using SSE instrinsics
long count_bits_intrinsic(const uchar *buffer, size_t bufsize)
{
    const size_t num_chunks = bufsize / chunk_size;
    const size_t chunked_bufsize = num_chunks * chunk_size;
    const int leftover = bufsize - chunked_bufsize;
    long total = 0;

#pragma omp parallel reduction (+:total)
    {
        long thread_total = 0;

#pragma omp for
        for (size_t i = 0; i < num_chunks; i++)
        {
            chunk_t chunk = *reinterpret_cast<chunk_t *>(buffer + i * chunk_size);
            thread_total += __builtin_popcountl(chunk);
        }

        total += thread_total;
    }

    total += count_bits_naive(buffer + chunked_bufsize, leftover);
    return total;
}

// Count the bits using inline ASM with SSE
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
        const long num_bits = count_bits_asm(mybuffer, bufsize_per_core);
        total += num_bits;
    }

    total += count_bits_naive(buffer + chunked_bufsize, leftover);

    return total;
}

// Count the bits using inline ASM with SSE for a buffer that is divisible by chunk_size
inline long count_bits_asm(const uchar *buffer, size_t bufsize)
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

int num_threads()
{
    int n_threads;
#pragma omp parallel
    {
#pragma omp master
        {
            n_threads = omp_get_num_threads();
        }
    }
    return n_threads > 0 ? n_threads : -1;
}


// Time how fast a bit counting function is
void time_bit_counting(const char *description, bit_counting_function *func, const uchar *buffer, size_t bufsize, int iters = fast_iters)
{
    // How many iterations represent roughly 10% of the total.
    // Used because We print a dot after every 10%.
    int ten_percent = iters / 10;
    if (ten_percent < 10)
        //  Just print a dot after every one
        ten_percent = 1;

    cout << "Timing " << description;
    const time_t start = time(NULL);
    for (int i = 0; i < iters; i++)
    {
        long num_bits = func(buffer, bufsize);
        if (i == 0)
            cout << " (" << num_bits << " bits are set) " << endl;
        else if (! (i % ten_percent))
            cout << ".";
    }
    const time_t duration = time(NULL) - start;
    cout << endl << ((double)duration / iters) << " seconds per iteration\n";
}

int main(int argc, char **argv)
{
    // Unbuffered stdout
    setvbuf(stdout, NULL, _IONBF, 0);

    // Figure out how much data we want
    size_t megs_of_data = 100; 
    if (argc > 1)
    {
        megs_of_data = atol(argv[1]);
    }
    cout << "Using " << megs_of_data << " megs of data" << endl;
    size_t bufsize = megs_of_data * 1024 * 1024;
    uchar *buffer = new unsigned char[bufsize];

    // Let's make the data unaligned so it's even harder for SSE
    // who sometimes cares about such things
    uchar *original_buffer = buffer;
    buffer += 1;
    bufsize -= 1;

    // Use /dev/urandom intead of /dev/random because
    // the latter may block if we try to read too much
    ifstream infile("/dev/urandom", ios::binary);
    cout << "Reading input..." << endl;
    infile.read(reinterpret_cast<char*>(buffer), bufsize);
    cout << "done reading input\n" << endl;
    infile.close();

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
