CC      = gcc
CFLAGS  = -Wall -Wextra -g -pthread

ADDRESSES ?= addresses.txt
REFERENCE ?=

vm: vm.o
	$(CC) $(CFLAGS) -o vm $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

run: vm
	./vm $(ADDRESSES) lru fifo

test: vm
	./tests/run_tests.sh "$(ADDRESSES)" "$(REFERENCE)"

clean:
	rm -f *.o vm output.txt correct.txt

.PHONY: run test clean
