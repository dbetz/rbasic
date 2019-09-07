SRCS=\
basic.c \
compile.c \
parse.c \
scan.c \
symbols.c \
debug.c \
system.c \
osint_posix.c


HDRS=\
compile.h \
image.h \
system.h \
types.h

rbasic:	$(SRCS) $(HDRS) Makefile
	$(CC) -DMAC -o $@ $(SRCS)

clean:
	rm -f rbasic
