CC = gcc
CFLAGS = -Wall -std=c99 -pedantic
MAIN = fs_emulator
OBJS = fs_emulator.o

all: $(MAIN)

$(MAIN): $(OBJS)
	$(CC) $(CFLAGS) -o $(MAIN) $(OBJS)

fs_emulator.o: fs_emulator.c
	$(CC) $(CFLAGS) -c fs_emulator.c

clean:
	rm -f *.o $(MAIN) core