all: httpd

httpd: httpd.c hashset.c
	gcc -W -Wall -lpthread -o httpd httpd.c hashset.c -lz -pthread

clean:
	rm httpd
