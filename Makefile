SRC = src
INCLUDE = include

CC = gcc
CFLAGS = -g -Wall -O2 -march=native -mtune=native -std=c99 
LDFLAGS = -pthread -lpthread
ADDFLAGS = -D_GNU_SOURCE 

#ADDFLAGS += -D_DEBUG	# enable debugging

# build both versions e.g., lock-based and lock-free (CAS) based implementation
all: fifo-lock.x fifo-cas.x

fifo-lock.x: $(SRC)/fifo.c
	$(CC) $(CFLAGS) -o $@ $(SRC)/fifo.c -I./$(INCLUDE) $(LDFLAGS) $(ADDFLAGS) -DLOCK


fifo-cas.x: $(SRC)/fifo.c
	$(CC) $(CFLAGS) -o $@ $(SRC)/fifo.c -I./$(INCLUDE) $(LDFLAGS) $(ADDFLAGS) -DCAS

.PHONY: clean

clean:
	    rm -f *.x *.out *.o *.a 
