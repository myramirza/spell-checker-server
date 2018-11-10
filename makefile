server: server.c
	gcc -pthread -Wall server.c -o server

clean:
	rm lltest $(objects)
