
/**
 * @file httpserver.c
 * @brief A simple HTTP 1.1 server that serves static files and generates Mandelbrot videos on the fly.
 * @author Zach Kohlman, kohlmanz@msoe.edu
 * @date 4/25/2024
 */

/**
 * Overall, this lab went really well. The only problem I had was sending the whole video of mandelbrot
 * once generated in one http request. Instead of sending all the data in one huge buffer, I send it in chuncks.
 * In hind sight, I could have saved a lot of time if I simply increased the size of the buffer.
 * This problem took up the majority of my time, the rest was using popen to write commands and run
 * the mandelbrot image generator, then using video software to generate a 2s video from the images.
 * I will say, using AI to help debug with memory leaks and HTML/JS problems helped A TON.
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h> 
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
static void build_response(char *url, char *content_type, int connection);
static void get_file_url(char *uri, char *url);
static void get_content_type(char *url, char *content_type);
static void generate_video(float x, float y);

// Max message to echo
#define MAX_MESSAGE 100000
#define MAX_EXT 16

/**
 * @brief The main routine that sets up the server and handles client connections.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line argument strings.
 * @return 0 on success, non-zero on failure.
 */
int main(int argc, char **argv)
{
	// locals
	int port = 80; // default port
	int sock;	   // socket descriptor
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
			/* Close when done */
			close(sock);

			/* Print the connection IP to console */
			char client_ip[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &(client_addr.sin_addr.s_addr), client_ip, INET_ADDRSTRLEN);
			printf("Client connected from IP address: %s\n", client_ip);

			/* Main HTTP routine */
			process_client(&client_addr, connection);
			printf("Done processing client\n");

			exit(EXIT_SUCCESS);
		}
		else
		{
			close(connection);
		}
	} // end of outer loop

	// will never get here
	return 0;
}

/**
 * @brief Processes a client request by parsing the request, building the response, and sending the data back to the client.
 * @param client_addr The address of the client that sent the request.
 * @param connection The socket descriptor for the client connection.
 */
static void process_client(struct sockaddr_in *client_addr, int connection)
{
    // Read client request by storing into buffer from read
    char *buffer = calloc(MAX_MESSAGE, sizeof(char));

    /* Read from port */
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

    /* Get the file url */
    get_file_url(uri, url);
    printf("URL %s\n", url);

    /* Get file type */
    get_content_type(url, content_type);

    // if (strstr(url, "mandel.html"))
    // {
	// 	printf("Verb %s\n", verb);
    //     /* Create server response  && send data in chuncks */
    //     build_response(url, content_type, connection);
    // }

    if (strstr(url, "mandel") && !strcmp(verb, "POST"))
    {
		printf("Verb %s\n", verb);
        // Read the POST request body

        // Parse the x and y values from the request body
		printf("buffer: %s\n\n", buffer);

		float x, y;
		// Parse the POST request body
		char *x_pos = strstr(buffer, "x=");
		char *y_pos = strstr(buffer, "y=");
		if (x_pos && y_pos)
		{
			x = atof(x_pos + 2);
			y = atof(y_pos + 2);
		}
		else
		{
			printf("Error: x and/or y not found in POST request\n");
		}

		printf("x: %f y: %f\n", x, y);

        /* Generate mandel video */
        generate_video(x, y);

        /* Create server response  && send data in chuncks */
        build_response(url, content_type, connection);

    }
    else
    {
        /* Create server response  && send data in chuncks */
        build_response(url, content_type, connection);
    }

    /* Free buffer after done */
    free(buffer);
}
/**
 * @brief Builds the server response for an HTTP 1.1 request and sends the data in chunks.
 * @param url The requested URL.
 * @param content_type The content type of the requested resource.
 * @param connection The socket descriptor for the client connection.
 */
static void build_response(char *url, char *content_type, int connection)
{
	/* Local Vars */
	char file_path[MAX_MESSAGE];
	const char *READ_BINARY = "rb";

	/* Format file_path */
	snprintf(file_path, MAX_MESSAGE, "%s", url);

	/* Open requested page in read-binary mode */
	FILE *file = fopen(file_path, READ_BINARY);

	/* If Null, send 404 response header*/
	if (file == NULL)
	{
		char error_response[] = "HTTP/1.1 404 Not Found\r\n"
								"Content-Type: text/plain\r\n"
								"Content-Length: 0\r\n"
								"\r\n"
								"404 Not Found";
		send(connection, error_response, strlen(error_response), 0);
		return;
	}

	/* Response header if good */
	char response_header[MAX_MESSAGE];
	snprintf(response_header, MAX_MESSAGE,
			 "HTTP/1.1 200 OK\r\n"
			 "Content-Type: %s\r\n"
			 "Transfer-Encoding: chunked\r\n"
			 "\r\n",
			 content_type);

	/* Send header */
	send(connection, response_header, strlen(response_header), 0);

	/* Send data in chuncks for large videos */
	char chunk[MAX_MESSAGE];
	size_t bytes_read;

	while ((bytes_read = fread(chunk, 1, sizeof(chunk), file)) > 0)
	{ // Loop until the end of the file is reached
		/* Declare a character array to store the chunk header */
		char chunk_header[16];

		/* Generate the chunk header in hexadecimal format followed by a carriage return and newline */
		snprintf(chunk_header, sizeof(chunk_header), "%X\r\n", (unsigned int)bytes_read);

		/* Sendt the chunck header to the client */
		send(connection, chunk_header, strlen(chunk_header), 0);

		/* Send the chunck data to the client */
		send(connection, chunk, bytes_read, 0);

		/* Send a carriage return and newline after the chunk data */
		send(connection, "\r\n", 2, 0);
	}

	char end_chunk[] = "0\r\n\r\n";

	// Send the final chunk data over the network connection
	send(connection, end_chunk, strlen(end_chunk), 0);

	fclose(file);
}

/**
 * @brief Gets the URL and file extension from the requested URI.
 * @param uri The requested URI.
 * @param url The output buffer to store the URL.
 */
static void get_file_url(char *uri, char *url)
{
	/* Local Vars */
	const char *BASE_PATH = "httpdocs";

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
 * @brief Sets the content type based on the file extension of the requested URL.
 * @param url The requested URL.
 * @param content_type The output buffer to store the content type.
 */
static void get_content_type(char *url, char *content_type)
{
	// Local vars
	const char *JPG = ".jpg";
	const char *MP4 = ".mp4";

	// Locate ext by dot
	const char *ext = strrchr(url, '.');

	if (!ext)
	{
		strcpy(content_type, "text/html");
	}
	else if (!strcmp(ext, JPG))
	{
		strcpy(content_type, "image/jpeg");
	}
	else if (!strcmp(ext, MP4))
	{
		strcpy(content_type, "video/mp4");
	}
	else
	{
		strcpy(content_type, "text/html");
	}
}

/**
 * @brief Generates a Mandelbrot video with random center values.
 */
static void generate_video(float x, float y)
{
	// Generate random mandelbro image
	char command[1000];
	const char *READ = "r";

	// Put into command field
	snprintf(command, sizeof(command), "cd /home/kohlmanz/dev/mandelbrot-zachkohl1 && ./mandelmovie -c 10 -m 100 -x %f -y %f", x, y);

	// Run command via read-only pipe and return
	FILE *f = popen(command, READ);
	pclose(f);

	// Create and copy video commands
	const char *create_video = "cd /home/kohlmanz/dev/mandelbrot-zachkohl1 && ffmpeg -y -i mandel%d.jpg mandel.mp4";
	const char *copy_video = "cp mandel.mp4 ~/networking_dev/HTTP-1.1-Server/httpdocs/";

	strcpy(command, create_video);
	strcat(command, " && ");
	strcat(command, copy_video);
	printf("Command: %s\n", command);

	// Create the mp4 video and copy into httpdocs
	FILE *vid = popen(command, READ);
	pclose(vid);
}