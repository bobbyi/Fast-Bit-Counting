count_bits: sse.cpp
	g++ -O3 -fopenmp -lgomp $< -o $@

test: count_bits
	./count_bits 20

clean:
	rm -f count_bits core

.PHONY: clean test
