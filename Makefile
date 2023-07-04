CC = gcc


all: hw2-thread

hw2-thread: hw2-thread.c
	$(CC) -pthread -o hw2-thread hw2-thread.c


clean:
	rm -f hw2-thread