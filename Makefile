CC=g++
CFLAGS=-Wall -Werror -O3 -fopenmp -lgomp

count_bits: sse.cpp
	$(CC) $(CFLAGS) $< -o $@

test: count_bits
	./count_bits 20

clean:
	rm -f count_bits core*

.PHONY: clean test
