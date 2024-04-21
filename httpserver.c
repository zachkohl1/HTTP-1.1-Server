// Simple TCP echo server

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>

static void process_client(struct sockaddr_in *client_addr, int connection);
static void build_response(char* url, char* file_ext, char* response, int* response_len);
static void get_file_url(char* uri, char* url, char* file_ext);

// Max message to echo
#define MAX_MESSAGE 100000
#define MAX_EXT	10

/* server main routine */

int main(int argc, char **argv)
{

	// locals
	unsigned short port = 80; // default port
	int sock;					// socket descriptor
	pid_t pid;

	// Was help requested?  Print usage statement
	if (argc > 1 && ((!strcmp(argv[1], "-?")) || (!strcmp(argv[1], "-h"))))
	{
		printf("\nUsage: tcpechoserver [-p port] port is the requested \
 port that the server monitors.  If no port is provided, the server \
 listens on port 3300.\n\n");
		exit(0);
	}

	// get the port from ARGV
	if (argc > 1 && !strcmp(argv[1], "-p"))
	{
		if (sscanf(argv[2], "%hu", &port) != 1)
		{
			perror("Error parsing port option");
			exit(0);
		}
	}

	// ready to go
	printf("tcp echo server configuring on port: %d\n", port);

	// for TCP, we want IP protocol domain (PF_INET)
	// and TCP transport type (SOCK_STREAM)
	// no alternate protocol - 0, since we have already specified IP

	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("Error on socket creation");
		exit(1);
	}

	// lose the pesky "Address already in use" error message
	int yes = 1;

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
	{
		perror("setsockopt");
		exit(1);
	}

	// establish address - this is the server and will
	// only be listening on the specified port
	struct sockaddr_in sock_address, client_addr;

	// address family is AF_INET
	// our IP address is INADDR_ANY (any of our IP addresses)
	// the port number is per default or option above

	sock_address.sin_family = AF_INET;
	sock_address.sin_addr.s_addr = htonl(INADDR_ANY);
	sock_address.sin_port = htons(port);

	// we must now bind the socket descriptor to the address info
	if (bind(sock, (struct sockaddr *)&sock_address, sizeof(sock_address)) < 0)
	{
		perror("Problem binding");
		exit(-1);
	}

	// extra step to TCP - listen on the port for a connection
	// willing to queue 5 connection requests
	if (listen(sock, 5) < 0)
	{
		perror("Error calling listen()");
		exit(-1);
	}

	// go into forever loop and echo whatever message is received
	// to console and back to source
	// Create a child process for each connection
	int connection;
	while (1)
	{
		socklen_t client_addr_len = sizeof(client_addr);

		// hang in accept and wait for connection
		printf("====Waiting====\n");
		if ((connection = accept(sock, (struct sockaddr *)&client_addr, &client_addr_len)) < 0)
		{
			perror("Error calling accept");
			exit(-1);
		}
		// Create child process
		pid = fork();
		if (pid < 0)
		{
			perror("ERROR on fork");
			close(sock);
			close(connection);
			exit(-1);
		}
		else if (!pid)
		{
			process_client(&client_addr, connection);
			printf("Done processing clinet\n");
			close(sock);
		}
		else
		{
			close(connection);
		}
	} // end of outer loop

	// will never get here
	return (0);
}

static void process_client(struct sockaddr_in *client_addr, int connection)
{
	// Read client GET request by storing into buffer from read
	char *buffer = calloc(MAX_MESSAGE, sizeof(char));
	int bytes_read = read(connection, buffer, MAX_MESSAGE - 1);	
	/* Determine if read was successful */
	if (bytes_read == 0)
	{ // socket closed
		printf("====Client Disconnected====\n");
		close(connection);
		free(buffer);
		return;
	}

	// see if client wants us to disconnect
	if (strncmp(buffer, "quit", 4) == 0)
	{
		printf("====Server Disconnecting====\n");
		close(connection);
		free(buffer);
		return;
	}

	/* Some local vars */
	char verb[MAX_MESSAGE], uri[MAX_MESSAGE], version[MAX_MESSAGE], url[MAX_MESSAGE], file_ext[MAX_EXT];

	/* Parse through request with sscanf and putting into above fields */
	sscanf(buffer, "%s %s %s", verb, uri, version);
	printf("Verb: %s URI: %s Version: %s\n", verb, uri, version);

	/* Get the file url */
	get_file_url(uri, url, file_ext);
	
	char* response = (char*)calloc(MAX_MESSAGE, sizeof(char));
	int response_len = 0;

	/* Create server response */
	build_response(url, file_ext, response, &response_len);
	printf("Response: %s\n", response);

	/* send HTTP response to client */
	send(connection, response, response_len, 0);

	free(buffer);
} // end of accept inner-while

/**
 * Builds the server response field for HTTP 1.1
*/
static void build_response(char* url, char* file_ext, char* response, int* response_len)
{
    char* http_header = (char*)calloc(MAX_MESSAGE, sizeof(char));
    snprintf(http_header, MAX_MESSAGE,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "\r\n",
             file_ext);

    // Construct the file path by combining the base path and the URL
    char file_path[MAX_MESSAGE];
    snprintf(file_path, MAX_MESSAGE, "httpdocs/%s", url);

	// Test hardcode
//	snprintf(file_path, MAX_MESSAGE, "/home/kohlmanz/networking_dev/HTTP-1.1-Server/httpdocs/%s", url);    // snprintf(file_path, MAX_MESSAGE, "httpdocs/%s", url);

    /* Open file Stream in READ-ONLY*/
    int fd = open(file_path, O_RDONLY);

    printf("fd: %i\n", fd);

	 /* File Failed to open... 404 error*/
	 if(fd < 0)
	 {
		snprintf(response, MAX_MESSAGE,
		"HTTP/1.1 404 Not Found\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"404 Not Found");

		*response_len = strlen(response);
		return;
	 }

	 /* Get file size with fstat */
	 struct stat file_info;
	 fstat(fd, &file_info);
	
	 // Get file size
	 off_t file_size = file_info.st_size;

	// Set response and response_len
	memcpy(response, http_header, strlen(http_header));
	*response_len = strlen(http_header);

	// Put file into buffer
	ssize_t bytes_read;
	while((bytes_read = read(fd, response + *response_len, MAX_MESSAGE-*response_len)) > 0)
	{
		*response_len += bytes_read;
	}
	free(http_header);
	close(fd);
}


/**
 * Sets URL and file_exntesion
 * Formats URL by removing all special cases (algorithm borrowed)
 * Removes URL parameters by looking for last ? and replacing with null terminator
*/
static void get_file_url(char* uri, char* url, char* file_ext)
{
    /* Local Vars */
    const char* DEFAULT_EXT = ".html";
	const char* BASE_PATH = "";

	// Init the url string
	strcpy(url, "");

	// Prepend the base path
	strcat(url, BASE_PATH);

    // Check if the URI starts with a forward slash
    if (uri[0] != '/')
    {
        strcat(url, "/");
    }

    strcat(url, uri);

    /* Locate URL parameters, and remove */
    char* url_parameters = strrchr(uri, '?');
    if (url_parameters)
    {
        *url_parameters = '\0';
    }

    /* Set file extension var */
    file_ext = strrchr(uri, '.');

    /* Default to .html if no extension provided */
    if (!file_ext || file_ext == url)
    {
        strcpy(file_ext, DEFAULT_EXT);
        strcat(url, file_ext);
    }

    printf("File ext: %s\n", file_ext);

	/**
	 * The following encoding algorithm taken from 
	 * URL https://gist.github.com/jesobreira/4ba48d1699b7527a4a514bfa1d70f61a
	*/
    // const char *hex = "0123456789abcdef";

    // int pos = 0;
    // for (int i = 0; i < strlen(uri); i++) {
    //     if (('a' <= uri[i] && uri[i] <= 'z')
    //         || ('A' <= uri[i] && uri[i] <= 'Z')
    //         || ('0' <= uri[i] && uri[i] <= '9')) {
    //             url[pos++] = uri[i];
    //         } else {
    //             url[pos++] = '%';
    //             url[pos++] = hex[uri[i] >> 4];
    //             url[pos++] = hex[uri[i] & 15];
    //         }
    // }
    // url[pos] = '\0';
	printf("URL: %s\n", url);
}