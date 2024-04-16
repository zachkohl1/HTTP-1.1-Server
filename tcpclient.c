#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>

// Max message to echo
#define MAX_MESSAGE     (int)       1000
#define LAB_BROADCAST   (in_addr_t) 0xC0A818FF
#define HOME_BROADCAST  (in_addr_t) 0xC0A801FF
#define DEFAULT_PORT    (int)       3300
#define DEFAULT_MSG     (char*) "Hello World!"

int main(int argc, char** argv)
{
    struct sockaddr_in server;
    int sock = 0;
    int port = DEFAULT_PORT;
    char c = '\0';     // - char
    char* buff = (char*)calloc(MAX_MESSAGE, sizeof(char));

    // Modification: Removed user ability to change output file name
    // -g for tftp get
    // -i for server ip address in dotted decimal
    // -f for file name
    while ((c = getopt(argc, argv, "h:i:p:m:"))!= -1)
    {
        switch (c)
        {
            case 'i':
                if(!inet_pton(AF_INET,optarg,&(server.sin_addr)))
                {
                    printf("Improper IP address\n");
                    free(buff);
                    exit(-1);
                }
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'h':
                free(buff);
                exit(-1);
                break;
            case 'm':
                strcpy(buff, optarg);
                break;
            default:
                perror("Error");
                free(buff);
                exit(-1);
         }
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    printf("TCP echo Server(s) configuring on port %d\n", ntohs(server.sin_port));

    sock = socket(AF_INET, SOCK_STREAM, 0);
    
    // 1s timeout
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt");
        free(buff);
        close(sock);
        exit(-1);
    }

    if(sock == -1)
    {
        printf("Socket creation failed\n");
        free(buff);
        close(sock);
        exit(-1);
    }

    if(connect(sock, (struct sockaddr*)&server,sizeof(server)) < 0)
    {
        perror("Error calling connect\n");
        free(buff);
        close(sock);
        exit(-1);
    }

    if(!buff)
    {
        // Default message
        strcpy(buff, DEFAULT_MSG);
    }



    while (1)
    {
        // Write message to TCP server
        if (write(sock, buff, strlen(buff) + 1) < 0)
        {
            perror("Write error");
            close(sock);
            free(buff);
            exit(-1);
        }

        // Read reply from server
        int bytes_read = read(sock, buff, MAX_MESSAGE - 1);

        if (!bytes_read)
        {
            break;
        }
        // Null-terminate received data
        (bytes_read <= MAX_MESSAGE) ? (buff[bytes_read] = '\0') : (buff[MAX_MESSAGE- 1] = '\0');

        // Print reply from server
        printf("Server reply: %s\n", buff);

        // Clear buffer for next input
        memset(buff, 0, MAX_MESSAGE);

        // Get input from user
        printf("Msg>: ");
        fgets(buff, MAX_MESSAGE, stdin);
        printf("\n");
    }   

    close(sock);

    free(buff);

    return 0;
}
