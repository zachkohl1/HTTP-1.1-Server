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
#include <signal.h>
#include <sys/wait.h>


static void process_client(struct sockaddr_in *client_addr, int connection);
static void build_response(char *url, char *content_type, char *response, int *response_len);
static void get_file_url(char *uri, char *url);
static void get_content_type(char* url, char* content_type);

// Max message to echo
#define MAX_MESSAGE 100000
#define MAX_EXT 16

/* server main routine */

int main(int argc, char **argv)
{

	// locals
	int port = 80; // default port
	int sock;				  // socket descriptor
	pid_t pid;

	// Was help requested?  Print usage statement
	if (argc > 1 && ((!strcmp(argv[1], "-?")) || (!strcmp(argv[1], "-h"))))
	{
		printf("\nUsage: httpserver [-p port] port is the requested \
 port that the server monitors.  If no port is provided, the server \
 listens on port 80.\n\n");
		exit(EXIT_SUCCESS);
	}

	// get the port from ARGV
	if (argc > 1 && !strcmp(argv[1], "-p"))
	{
		if (sscanf(argv[2], "%i", &port) != 1)
		{
			perror("Error parsing port option");
			exit(EXIT_FAILURE);
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
		exit(EXIT_FAILURE);
	}

	// lose the pesky "Address already in use" error message
	int yes = 1;

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
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
		exit(EXIT_FAILURE);
	}

	// extra step to TCP - listen on the port for a connection
	// willing to queue 5 connection requests
	if (listen(sock, 5) < 0)
	{
		perror("Error calling listen()");
		exit(EXIT_FAILURE);
	}

	// go into forever loop and echo whatever message is received
	// to console and back to source
	// Create a child process for each connection
	int connection;
	int child_status;
	while (1)
	{
		socklen_t client_addr_len = sizeof(client_addr);

		// hang in accept and wait for connection
		printf("====Waiting====\n");
		if ((connection = accept(sock, (struct sockaddr *)&client_addr, &client_addr_len)) < 0)
		{
			perror("Error calling accept");
			exit(EXIT_FAILURE);
		}
		// Create child process
		pid = fork();
		if (pid < 0)
		{
			perror("ERROR on fork");
			close(sock);
			close(connection);
			exit(EXIT_FAILURE);
		}
		else if (!pid)
		{
			char client_ip[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &(client_addr.sin_addr.s_addr), client_ip, INET_ADDRSTRLEN);
			printf("Client connected from IP address: %s\n", client_ip);

			process_client(&client_addr, connection);
			printf("Done processing clinet\n");
			close(sock);
			exit(EXIT_SUCCESS);
		}
		else
		{
			close(connection);
			pid_t terminated = waitpid(pid, &child_status,0);

			if(terminated == -1)
			{
				perror("waitpid");
			} else {
				if(WIFEXITED(child_status)){
            		WEXITSTATUS(child_status);
				}
			}
		}
	} // end of outer loop

	// will never get here
	return 0;
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
	char verb[MAX_MESSAGE], uri[MAX_MESSAGE], version[MAX_MESSAGE], url[MAX_MESSAGE], content_type[MAX_EXT];

	/* Parse through request with sscanf and putting into above fields */
	sscanf(buffer, "%s %s %s", verb, uri, version);
//	printf("Verb: %s URI: %s Version: %s\n", verb, uri, version);

	/* Get the file url */
	get_file_url(uri, url);

	/* Get file type */
	get_content_type(url, content_type);

	char *response = (char *)calloc(MAX_MESSAGE, sizeof(char));
	int response_len = 0;

	/* Create server response */
	build_response(url, content_type, response, &response_len);
	printf("Response: %s\n", response);

	/* send HTTP response to client */
	send(connection, response, response_len, 0);

	free(buffer);
} // end of accept inner-while

/**
 * Builds the server response field for HTTP 1.1
 */
static void build_response(char* url, char* content_type, char* response, int* response_len)
{
	// Construct the file path by combining the base path and the URL
	char file_path[MAX_MESSAGE];
	snprintf(file_path, MAX_MESSAGE, "%s", url);

	/* Open file Stream as read-only binary */
    FILE* file = fopen(url, "rb");
    if (file == NULL) {
		snprintf(response, MAX_MESSAGE,
				 "HTTP/1.1 404 Not Found\r\n"
				 "Content-Type: text/plain\r\n"
				 "Content-Length: 0\r\n"
				 "\r\n"
				 "404 Not Found");

		*response_len = strlen(response);
        return;
    }

    // Get the file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    // Set the response header
    snprintf(response, MAX_MESSAGE,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "\r\n",
             content_type, file_size);

    *response_len = strlen(response);

    // Read the file contents into the response buffer
    size_t bytes_read = fread(response + *response_len, 1, file_size, file);
    *response_len += bytes_read;

    fclose(file);
}

/**
 * Sets URL and file_exntesion
 * Formats URL by removing all special cases (algorithm borrowed)
 * Removes URL parameters by looking for last ? and replacing with null terminator
 */
static void get_file_url(char *uri, char *url)
{
	/* Local Vars */
	const char *BASE_PATH = "httpdocs/";

	// Init the url string
	memset(url, '\0', MAX_MESSAGE);

	// Prepend the base path
	strcat(url, BASE_PATH);
	strcat(url, uri);

	/* Locate URL parameters, and remove */
	char *url_parameters = strrchr(uri, '?');
	if (url_parameters)
	{
		*url_parameters = '\0';
	}
}

/**
 * Sets the contnent type based on the file extension
 * Only JPG and HTML supported. Defaults to .HTML
*/
static void get_content_type(char* url, char* content_type)
{
	// Local vars
	const char* JPG = ".jpg";

	// Locate ext by dot
	const char* ext = strrchr(url, '.');

	if(!ext){
		strcpy(content_type, "text/html");
	}  else if(!strcmp(ext, JPG)){
		strcpy(content_type, "image/jpeg");
	} else{
		strcpy(content_type, "text/html");
	}
}