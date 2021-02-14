CFLAGS += -Wall -Werror

binaries := cathy

cathy: cathy.o file.o filerepo.o hasher.o ioread.o outdir.o util.o

.PHONY: all
all: $(binaries)

clean:
	rm -f *.o ${binaries}
