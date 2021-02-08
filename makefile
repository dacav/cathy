CFLAGS = -Wall -Werror

binaries := cathy

.PHONY: all
all: $(binaries)

clean:
	rm -f *.o ${binaries}
