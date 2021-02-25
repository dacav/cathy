CFLAGS += -Wall -Werror -Wextra

binaries := cathy

cathy: cathy.o events.o file.o filerepo.o hasher.o ioread.o outdir.o util.o

PATH := ${PWD}:${PATH}
test: $(binaries)
	sh test.sh

.PHONY: all
all: $(binaries)

clean:
	rm -f *.o ${binaries}
