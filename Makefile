CC=g++
CFLAGS=-Wall -Werror -O3 -fopenmp -lgomp
EXECUTABLE=./count_bits

$(EXECUTABLE): sse.cpp
	$(CC) $(CFLAGS) $< -o $@

test: $(EXECUTABLE)
	$(EXECUTABLE) 20

clean:
	rm -f $(EXECUTABLE) core*

.PHONY: clean test
