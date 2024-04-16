// Simple TCP echo server

#include <stdio.h>      
#include <stdlib.h>     
#include <unistd.h>  
#include <string.h>	
#include <sys/types.h>  
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>  
#include <pthread.h>


// Max message to echo
#define MAX_MESSAGE	1000

/* server main routine */

int main(int argc, char** argv) {

	// locals
	unsigned short port = 3300; // default port
	int sock; // socket descriptor
	pid_t pid;

	// Was help requested?  Print usage statement
	if (argc > 1 && ((!strcmp(argv[1],"-?"))||(!strcmp(argv[1],"-h"))))
	{
		printf("\nUsage: tcpechoserver [-p port] port is the requested \
 port that the server monitors.  If no port is provided, the server \
 listens on port 3300.\n\n");
		exit(0);
	}
	
	// get the port from ARGV
	if (argc > 1 && !strcmp(argv[1],"-p"))
	{
		if (sscanf(argv[2],"%hu",&port)!=1)
		{
			perror("Error parsing port option");
			exit(0);
		}
	}
	
	// ready to go
	printf("tcp echo server configuring on port: %d\n",port);
	
	// for TCP, we want IP protocol domain (PF_INET)
	// and TCP transport type (SOCK_STREAM)
	// no alternate protocol - 0, since we have already specified IP
	
	if ((sock = socket( PF_INET, SOCK_STREAM, 0 )) < 0) 
	{
		perror("Error on socket creation");
		exit(1);
	}
  
  	// lose the pesky "Address already in use" error message
	int yes = 1;

	if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) 
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
	if (bind(sock, (struct sockaddr *) &sock_address, sizeof(sock_address))<0)
	{
		perror("Problem binding");
		exit(-1);
	}
	
	// extra step to TCP - listen on the port for a connection
	// willing to queue 5 connection requests
	if ( listen(sock, 5) < 0 ) 
	{
		perror("Error calling listen()");
		exit(-1);
	}
	
	// go into forever loop and echo whatever message is received
	// to console and back to source
	char* buffer = calloc(MAX_MESSAGE,sizeof(char));
	int bytes_read;
	int echoed;
	int connection;
	
    while (1) {			
		socklen_t client_addr_len = sizeof(client_addr);

		// hang in accept and wait for connection
		printf("====Waiting====\n");
		if((connection = accept(sock, (struct sockaddr *)&client_addr, &client_addr_len)) < 0) 
		{
			perror("Error calling accept");
			exit(-1);
		}		

		// Create child process
		pid = fork();

		if(pid < 0)
		{
			perror("ERROR on fork");
		} else if(!pid){
			close(sock);
			// ready to r/w - another loop - it will be broken when
			// the connection is closed
			while(1)
			{
				buffer[0] = '\0'; 	// guarantee a null here to break out on a
									// disconnect

				// read message								
				bytes_read = read(connection,buffer,MAX_MESSAGE-1);
							
				if (bytes_read == 0)
				{	// socket closed
					printf("====Client Disconnected====\n");
					close(connection);
					break;  // break the inner while loop
				}

				// make sure buffer has null temrinator
				buffer[bytes_read] = '\0';
							
				// see if client wants us to disconnect
				if (strncmp(buffer,"quit",4)==0)
				{
					printf("====Server Disconnecting====\n");
					close(connection);
					break;  // break the inner while loop
				}
				
				// Print received message and client IP
				printf("Received message from: %s\nPort: %i\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
				printf("Message: %s\n", buffer);

				// send it back to client
				if ((echoed = write(connection, buffer, bytes_read + 1)) < 0 )
				{
					perror("Error sending echo");
					exit(-1);
				}
				else
				{			
					printf("Bytes echoed: %d\n",echoed);
				}
					
			}  // end of accept inner-while
		} else {
			close(connection);
		}
    }	// end of outer loop
	free(buffer);
	// will never get here
	return(0);
}