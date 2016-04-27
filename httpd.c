/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <zlib.h>
#include <pthread.h>
#include <semaphore.h>
#include "hashset.h"

#define TABLE 32768
//#define TABLE 7

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

struct parametry{
        int client;
	hashset_t set;
};

void * accept_request(void*);
void bad_request(int);
void cat(int, FILE *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *, const char *);
int get_line(int, char *, int);
void headers(int, const char *);
void not_found(int);
void serve_file(int, const char *);
int startup(u_short *);
void unimplemented(int);
void sent_count(int client, int count);
void sent_OK(int client);
int inf(const void *src, int srcLen, void *dst, int dstLen) ;
int parse_words(char * buf, int* first, int buf_size);
hashset_t set;
sem_t sem;

void print_decomp(char * buf, int first, int decomprimed, int buf_size){
	int i;
	if (first+decomprimed<=buf_size){
		for (i=first;i<first+decomprimed;i++){
			printf("index=%d znak=%c\n",i,buf[i]);
		}
	}else{
		for (i=first;i<buf_size;i++){
			printf("index=%d znak=%c\n",i,buf[i]);
		}
		for (i=0;i<first+decomprimed-buf_size;i++){
			printf("index=%d znak=%c\n",i,buf[i]);
		}
	}
}

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
#define PACKET 1500
#define DECOMP_BUF_SIZE 50000
void * accept_request(void *  param){
 	char buf[1024];
	char buf2[1024];
	char decomp[DECOMP_BUF_SIZE]={'k'};
 	char method[255];
 	char url[255];
	char path[512];
	int numchars;
 	size_t i, j;
	unsigned char k,l;
	struct stat st;
	int length=0;	
	char data_buf[PACKET];
	int len;
	z_stream strm  = {0};
	int ret;
	int next_word;
	int out_space; 
	int pred_infl;
	int dekomprimovano;
	int index_decomp;
	int prijato;
	char zlib_out[DECOMP_BUF_SIZE];
	int celkem=0;
	struct parametry * par = (struct parametry *)param;

 	numchars=get_line(par->client, buf, sizeof(buf)); /* POST /osp/myserver/data HTTP/1.1 */
	printf("head=%s\n",buf);

	i=0;	
	j=0;

 	while (!ISspace(buf[j]) && (i < sizeof(method) - 1)){
  		method[i] = buf[j];
  		i++;
		j++;
 	}
 	method[i] = '\0';

 	if (strcasecmp(method, "GET") && strcasecmp(method, "POST")){
  		unimplemented(par->client);
  		close(par->client);
		free(par);
		pthread_exit(0);
 	}
 	
	/* preskocime mezery */
	while (ISspace(buf[j]) && (j < sizeof(buf))){
		j++;
	}

	/* URL pozadavku - resource */
	i = 0; 	
	while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf))){
		url[i] = buf[j];
		i++;
		j++;
	}
	url[i] = '\0';

	if (strcasecmp(method, "POST") == 0){
		/* chybny resource*/
		if (strcmp(url,"/osp/myserver/data")){
 			close(par->client);
			free(par);
			pthread_exit(0);
		}
		k=0;
		while(1){
			get_line(par->client, buf2, sizeof(buf2));
			/*newlajna pred prilohou*/
			if (buf2[0]=='\n'){
				break;
			}
			/* delka prilohy je ve 4. lajne hlavicky*/
			if (k==3){
				l=16; /*delka prilohy je na 16 pozici*/
				while (buf2[l]!='\n' && buf2[l]!='\r' 
						&& buf2[l]!='\0' ){
					length*=10;
					length+=buf2[l]-48;
					l++;
				}	
			}
			k++;
		}
		printf("komprimovano=%d\n",length);

		inflateInit2(&strm,15 | 32);

		strm.next_in = (unsigned char*)data_buf;
		strm.avail_out = DECOMP_BUF_SIZE;
		strm.next_out = (unsigned char*)zlib_out; //adresa prvnoho byte pro dekomp data

		next_word = 0;
		out_space = DECOMP_BUF_SIZE;
		index_decomp = 0;

		/* DEKOMPRESE - TODO kontrolovat, zda si neprepisuju stara data  */
		/* zatim doufam, ze delka vystupnihu bufferu je dostatecne velkym*/
		/* nasobkem delky vstupniho bufferu 				 */
		while (length){
			//bajty, ktere chci precist
			len = PACKET < length ? PACKET : length; 
			prijato=recv(par->client,data_buf,len,0);
			length-=prijato;
			//printf("prijato=%d, pozadovano=%d\n",prijato, len);

			strm.avail_in = prijato; //pocet bajtu k dekompresi
			strm.avail_out = DECOMP_BUF_SIZE;
			strm.next_out = (unsigned char*)zlib_out;
			strm.next_in = (unsigned char*)data_buf;	

			/* updates next_in, avail_in, next_out, avail_out */	
			pred_infl = strm.avail_out;		
			ret = inflate(&strm, Z_SYNC_FLUSH);
			dekomprimovano = pred_infl-strm.avail_out;
			
			for (i=0;i<dekomprimovano;i++){
			//	printf("indx=%d, znk=%c\n",i,zlib_out[i]);
				decomp[index_decomp++]=zlib_out[i];
				index_decomp%=DECOMP_BUF_SIZE;	
			}

			
			//print_decomp(decomp, index_decomp, dekomprimovano,DECOMP_BUF_SIZE );
			out_space-=dekomprimovano;
			celkem+=dekomprimovano;
			//printf("dekopmrimovano=%d celkem=%d\n",dekomprimovano,celkem);

			/* dosel vstupni buffer */
			if (ret == Z_OK){
				/* vse uspesne precteno, nastavim input na zacatek*/
				//printf("\nok, out_space=%d\n",out_space);
			}else if (ret == Z_STREAM_END){
				if (length){
					printf("\nkonec streamu pred koncem dat\n");
					while (length){
						//bajty, ktere chci precist
						len = PACKET < length ? PACKET : length; 
						prijato=recv(par->client,data_buf,len,0);
						length-=prijato;
					}
				}else{
					printf("\nkonec streamu s koncem dat\n");
				}
			/* dosel vystupni buffer */
			}else if (ret ==  Z_BUF_ERROR){
				printf("\nbuff error\n");
				printf("available input=%d\n",strm.avail_in);
			}else if (ret == Z_DATA_ERROR){
				printf("\ndata error\n");
			}else if (ret == Z_MEM_ERROR){
				printf("\nmem error\n");
			}else if (ret == Z_STREAM_ERROR){
				printf("\nstream error\n");
			}
			out_space+=parse_words(decomp,&next_word,DECOMP_BUF_SIZE);
			//printf("outer space po parse=%d\n",out_space);	
		}
		inflateEnd(&strm);

		sent_OK(par->client); /*pokud tohle neodeslu pred zavrenim, klient
				si zahlasi :empty response: */
 		close(par->client);
		free(par);
		pthread_exit(0);
	}

	if (strcasecmp(method, "GET") == 0){
		if (!strcmp(url,"/osp/myserver/count")){
			printf("slova=%d\n",hashset_num_items(set));
			sent_count(par->client,hashset_num_items(set));
			hashset_destroy(set);
			set = hashset_create(TABLE);
			if (set == NULL) {
				printf("failed to create hashset instance\n");
			}
		}
 	}

	        if (stat(path, &st) == -1) {
                while ((numchars > 0) && strcmp("\n", buf)) { /* read & discard headers */
                        numchars = get_line(par->client, buf, sizeof(buf));
                }
                //not_found(client);
        }
	
 	close(par->client);
	free(par);
	pthread_exit(0);
}



/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
void cat(int client, FILE *resource)
{
 char buf[1024];

 fgets(buf, sizeof(buf), resource);
 while (!feof(resource))
 {
  send(client, buf, strlen(buf), 0);
  fgets(buf, sizeof(buf), resource);
 }
}


/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
void error_die(const char *sc)
{
 perror(sc);
 exit(1);
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
int get_line(int sock, char *buf, int size)
{
 int i = 0;
 char c = '\0';
 int n;

 while ((i < size - 1) && (c != '\n'))
 {
  n = recv(sock, &c, 1, 0);
  /* DEBUG printf("%02X\n", c); */
  if (n > 0)
  {
   if (c == '\r')
   {
    n = recv(sock, &c, 1, MSG_PEEK);
    /* DEBUG printf("%02X\n", c); */
    if ((n > 0) && (c == '\n'))
     recv(sock, &c, 1, 0);
    else
     c = '\n';
   }
   buf[i] = c;
   i++;
  }
  else
   c = '\n';
 }
 buf[i] = '\0';
 
 return(i);
}

/**********************************************************************/
/* Testovaci metoda - vraci pouze cislo */
/**********************************************************************/
void sent_count(int client, int count){
 	char buf[1024];

	sprintf(buf, "HTTP/1.0 200 OK\r\n");
 	send(client, buf, strlen(buf), 0);
 	sprintf(buf, SERVER_STRING);
 	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
 	send(client, buf, strlen(buf), 0);
 	sprintf(buf, "\r\n");
 	send(client, buf, strlen(buf), 0);

 	sprintf(buf, "%d\r\n",count);
	send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Odpoved klientovi - vse je OK */
/**********************************************************************/
void sent_OK(int client){
 	char buf[1024];

	sprintf(buf, "HTTP/1.0 200 OK\r\n");
 	send(client, buf, strlen(buf), 0);
 	sprintf(buf, SERVER_STRING);
 	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
 	send(client, buf, strlen(buf), 0);
 	sprintf(buf, "\r\n");
 	send(client, buf, strlen(buf), 0);
}



/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
int startup(u_short *port)
{
 int httpd = 0;
 struct sockaddr_in name;

 httpd = socket(PF_INET, SOCK_STREAM, 0);
 if (httpd == -1)
  error_die("socket");
 memset(&name, 0, sizeof(name));
 name.sin_family = AF_INET;
 name.sin_port = htons(*port);
 name.sin_addr.s_addr = htonl(INADDR_ANY);
 if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
  error_die("bind");
 if (*port == 0)  /* if dynamically allocating a port */
 {
  int namelen = sizeof(name);
  if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
   error_die("getsockname");
  *port = ntohs(name.sin_port);
 }
 if (listen(httpd, 5) < 0)
  error_die("listen");
 return(httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
void unimplemented(int client)
{
 char buf[1024];

 sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, SERVER_STRING);
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "Content-Type: text/html\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "</TITLE></HEAD>\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "</BODY></HTML>\r\n");
 send(client, buf, strlen(buf), 0);
}

/**********************************************************************/

int main(void){
 	int server_sock = -1;
 	u_short port = 8080;
 	int client_sock = -1;
 	struct sockaddr_in client_name;
 	int client_name_len = sizeof(client_name);

	pthread_t tid;          //identifikator vlakna
        pthread_attr_t attr;    //atributy vlakna
	struct parametry *p1;

        pthread_attr_init(&attr);       //inicializuj implicitni atributy
	sem_init(&sem, 0, 1); //thread-shared, init-value 1


 	server_sock = startup(&port);
 	printf("httpd running on port %d\n", port);

	set = hashset_create(TABLE);
	if (set == NULL) {
		printf("failed to create hashset instance\n");
             	close(server_sock);
		return 1;
        }
	
 	while (1){
  		client_sock = accept(server_sock,
                       (struct sockaddr *)&client_name,
                       &client_name_len);
  		if (client_sock == -1){
   			error_die("accept");
		}
		p1 = malloc(sizeof(struct parametry));
		p1->client = client_sock;
		p1->set = set;
		pthread_create(&tid, &attr, accept_request, (void*)p1);        //vytvor vlakno 
 	}
	sem_destroy(&sem);
 	close(server_sock);
	return(0);
}
/**********************************************************************/
/* Dekomprimuje obsah bufferu a ulozi ho do dalsihoi bufferu*/
/**********************************************************************/
int inf(const void *src, int srcLen, void *dst, int dstLen) {
    z_stream strm  = {0};
    strm.total_in  = strm.avail_in  = srcLen;
    strm.total_out = strm.avail_out = dstLen;
    strm.next_in   = (Bytef *) src;
    strm.next_out  = (Bytef *) dst;

    strm.zalloc = Z_NULL;
    strm.zfree  = Z_NULL;
    strm.opaque = Z_NULL;

    int err = -1;
    int ret = -1;

    err = inflateInit2(&strm, (15 + 32)); //15 window bits, and the +32 tells zlib to to detect if using gzip or zlib
    if (err == Z_OK) {
        err = inflate(&strm, Z_FINISH);
        if (err == Z_STREAM_END) {
            ret = strm.total_out;
        }
        else {
             inflateEnd(&strm);
             return err;
        }
    }
    else {
        inflateEnd(&strm);
        return err;
    }

    inflateEnd(&strm);
    return ret;
}
/********************************************************************/
/* Vrati pocet smazanych znaku     */
/********************************************************************/
int delete_word(char * buf, int first, int last, int buf_size){
	int i;
	int chars = 0;
	if (first<=last){
		for (i=first;i<=last;i++){
			buf[i]='\0';
			chars++;
		}
	}else{
		for (i=first;i<buf_size;i++){
			buf[i]='\0';
			chars++;
		}
		for (i=0;i<=last;i++){
			buf[i]='\0';
			chars++;
		}
	}
	return chars;
}
/**********************************************************************/
/* Dekomprimuje obsah bufferu a ulozi ho do dalsihoi bufferu          */
/* Vraci pozici prvniho bajtu prvniho nedokonceneho slova             */
/* Za kazdym slovem ocekavame space character -> isspace(char c)      */
/* Vraci, kolik znaku z fufferu bylo uvolneno                         */
/**********************************************************************/
int parse_words(char * buf, int* first, int buf_size){
	int main_index=*first; /*iterace pres bajty v bufferu*/
	int word_index=0; /* itrace pres bajty v aktualnim sloce */
	char word[126];
	int chars = 0;
	while(buf[main_index]!='\0'){
		if (isspace(buf[main_index])){
			//printf("pw mezera\n");
			if (word_index){ /*delka slova neni nula*/
				word[word_index]='\0'; /*ukoncim slovo*/
				sem_wait(&sem);
				hashset_add(set, (void *)word, word_index);
				sem_post(&sem);
				chars+=delete_word(buf, *first, main_index-1, buf_size);
				//printf("pw slovo= %s\n",word);
				word_index=0;
			}
			buf[main_index]='\0';
			/* v pripade, ze koncim mezerou musim dalsi 
			first index nastavit manualne protoze 
			jinak by ukazoval na zacatek posledniho
			- jiz smazaneho slova*/
			*first=(main_index+1)%buf_size; 
			chars++;
		}else{	
			/*prvni pismeno noveho slova*/
			if (!word_index) *first = main_index;
			//printf("pw znak=%c\n",buf[main_index]);
			word[word_index]=buf[main_index];
			word_index++;
		}
		main_index++;
		main_index%=buf_size;
	}
	return chars;
}