CFLAGS = -Wall -Wextra -g -pthread

vm: vm.o
	gcc $(CFLAGS) -o vm $^

%.o: %.c
	gcc $(CFLAGS) -c $<

run: vm
	./vm addresses.txt lru fifo

clean:
	rm -f *.o vm output.txt

.PHONY: run clean