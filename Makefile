server: server.c
	gcc server.c -o server -lpthread
clean:
	rm server
