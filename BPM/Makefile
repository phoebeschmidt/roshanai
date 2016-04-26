
all: BPMclient BPMserver

BPMclient: BPMclient.o
	$(CC) -o BPMclient BPMclient.o

BPMserver: BPMserver.o
	$(CC) -o BPMserver BPMserver.o

BPMclient.o: BPMclient.c
	$(CC) -c BPMclient.c

BPMserver.o: BPMserver.c
	$(CC) -c BPMserver.c

clean:
	rm -f *.o BPMclient BPMserver

