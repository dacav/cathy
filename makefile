CFLAGS = -Wall -Werror

binaries := cathy

cathy: cathy.o util.o

.PHONY: all
all: $(binaries)

clean:
	rm -f *.o ${binaries}
