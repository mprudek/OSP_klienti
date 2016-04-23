all: httpd

httpd: httpd.c hashset.c
	gcc -W -Wall -lpthread -o httpd httpd.c hashset.c -lz

clean:
	rm httpd
