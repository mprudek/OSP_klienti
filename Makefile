all: httpd

httpd: httpd.c
	gcc -W -Wall -lpthread -o httpd httpd.c -lz

clean:
	rm httpd
