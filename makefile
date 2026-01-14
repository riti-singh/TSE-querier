# Makefile for TSE querier
CC = gcc
CFLAGS = -Wall -pedantic -std=c11 -ggdb -I../libcs50 -I../common

LIBCS50 = ../libcs50/libcs50.a
COMMON  = ../common/common.a
INDEXOBJ = ../common/index.o

PROG = querier
OBJS = querier.o

# for memory-leak tests
VALGRIND = valgrind --leak-check=full --show-leak-kinds=all


.PHONY: all clean test valgrind

all: $(PROG)

$(PROG): $(OBJS) $(LIBCS50) $(COMMON) $(INDEXOBJ)
	$(CC) $(CFLAGS) $(OBJS) $(INDEXOBJ) $(COMMON) $(LIBCS50) -o $(PROG)

querier.o: querier.c
	$(CC) $(CFLAGS) -c querier.c

$(INDEXOBJ): ../common/index.c
	$(CC) $(CFLAGS) -c ../common/index.c -o $(INDEXOBJ)

test: $(PROG) testing.sh
	@bash testing.sh

# paths for testing
PDIR = /cs50/shared/tse/output/letters-1
IDX  = /cs50/shared/tse/output/letters-1.index

valgrind: $(PROG)
	$(VALGRIND) ./$(PROG) $(PDIR) $(IDX)


clean:
	rm -f $(PROG) $(OBJS)
