CFLAGS = -Wall -Werror

binaries := cathy

cathy: cathy.o ioread.o util.o

.PHONY: all
all: $(binaries)

clean:
	rm -f *.o ${binaries}
