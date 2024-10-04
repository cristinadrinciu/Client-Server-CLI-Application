#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <iostream>

#include "common.h"


void run_client(int sockfd, tcp_client client, struct sockaddr_in serv_addr) {
    int rc;

    // send the ID to the server
    tcp_message connect_msg;
    memset(&connect_msg, 0, sizeof(tcp_message));
    connect_msg.cmd = SERVER_CONNECT;
    strcpy(connect_msg.id, client.id);
    rc = send_all(sockfd, &connect_msg, sizeof(tcp_message));
    DIE(rc < 0, "send failed");

    // create a pollfd structure for the socket
    struct pollfd fds[2];

    fds[0].fd = sockfd; // first file descriptor is the socket
    fds[0].events = POLLIN;
    fds[1].fd = STDIN_FILENO; // the second file descriptor is the standard input
    fds[1].events = POLLIN;

    // create a buffer
    char buffer[MAX_BUFFER_LEN];

    while (1) {
        rc = poll(fds, 2, -1);
        DIE(rc < 0, "poll failed");

        // check if the socket is ready for reading
        if(fds[0].revents & POLLIN) {
            udp_message msg;
            // it receives a message from the server
            rc = recv_all(sockfd, &msg, sizeof(udp_message));

            if(rc == 0) {
                // the server has closed the connection
                break;
            }
            DIE(rc < 0, "recv failed");

            // print the message
            print_udp_message(msg);
        } else if (fds[1].revents & POLLIN) {
            // it receives a message from the standard input / will send a message to the server
            // read the input buffer
            memset(buffer, 0, MAX_BUFFER_LEN);
            fgets(buffer, MAX_BUFFER_LEN, stdin);

            // check if it is a vallid command
            int arg_nr;
            char *command[MAX_NR_ARGUMENTS];

            get_command_arguments(buffer, arg_nr, command);

            if (!is_command_valid(arg_nr, command)) {
                continue;
            }

            tcp_message msg;
            memset(&msg, 0, sizeof(tcp_message));
            strcpy(msg.id, client.id);
            strcpy(msg.ip, client.ip);
            msg.port = client.port;

            // check if it is an exit command
            if (strcmp(command[0], "exit") == 0) {
                // disconect the client
                msg.cmd = EXIT;

                // send the message to the server
                rc = send_all(sockfd, &msg, sizeof(tcp_message));
                DIE(rc < 0, "send failed");

                // shutdown the connection
                rc = shutdown(sockfd, SHUT_RDWR);
                DIE(rc < 0, "shutdown failed");

                // close the connection the created socket
                close(sockfd);

                exit(0);
            } else if (strcmp(command[0], "subscribe") == 0) {
                // build the message for the server
                msg.cmd = SUBSCRIBE;
                strcpy(msg.topic, command[1]);

                // add terminal character
                msg.topic[strlen(msg.topic)] = '\0';

                // send the message to the server
                rc = send_all(sockfd, &msg, sizeof(tcp_message));
                DIE(rc < 0, "send failed");

                printf("Subscribed to topic.\n");
                fflush(stdout);
            } else if (strcmp(command[0], "unsubscribe") == 0) {
                // build the message for the server
                msg.cmd = UNSUBSCRIBE;
                strcpy(msg.topic, command[1]);

                // add terminal character
                msg.topic[strlen(msg.topic)] = '\0';

                // send the message to the server
                rc = send_all(sockfd, &msg, sizeof(tcp_message));
                DIE(rc < 0, "send failed");

                printf("Unsubscribed from topic.\n");
                fflush(stdout);
            } else {
                printf("\nInvalid command\n");
            }
        }
    }
    
}

int main(int argc, char *argv[]) {
    // most of the following code is taken from lab 7
    if (argc != 4) {
        // print to stderr the fact that the usage is wrong
        fprintf(stderr, "\n Usage: %s <ID_CLIENT> <IP_SERVER> <PORT_SERVER>\n", argv[0]);
        return 1;
    }

    // deactivate the buffering of stdout
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    tcp_client client = {};
    client.is_connected = false;
    strcpy(client.id, argv[1]);
    strcpy(client.ip, argv[2]);
    client.port = atoi(argv[3]);

    // treat the server's port as a number
    uint16_t port;
    int rc = sscanf(argv[3], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    // create a socket for connecting to the server
    const int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    DIE(sockfd < 0, "socket");

    // deactivate Nagle's algorithm, so that the commands and messages are sent immediately
    int flag = 1;
    rc = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));

    // create the server's address structure
    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
    DIE(rc <= 0, "inet_pton");

    // connect to the server
    rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "connection failed");
    client.is_connected = true;

    run_client(sockfd, client, serv_addr);

    exit(0);
}
