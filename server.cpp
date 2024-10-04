#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include <netinet/tcp.h>
#include <unordered_set>
#include <vector>
#include <iostream>
#include <string>

#include "common.h"

#define MAX_CONNECTIONS 32

// declare global variables
// the key is the topic and the value is a vector of clients that are subscribed to the topic
std::unordered_map<std :: string, std :: vector<tcp_client*>> topics;

// a set for storing the clients that are connected to the server
std::unordered_set<tcp_client *> subscribers;

void connect_to_server(int listenfd, std::vector<struct pollfd> &fds, int &nfds) {
    // accept the incoming connection from a TCP subscriber
    struct sockaddr_in tcp_subscriber_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    memset(&tcp_subscriber_addr, 0, socket_len);

    int tcp_subscriberfd = accept(listenfd, (struct sockaddr *)&tcp_subscriber_addr, &socket_len);
    DIE(tcp_subscriberfd < 0, "accept failed");

    // add the new file descriptor to the pollfd structure
    const int flag = 1;
    int rc = setsockopt(tcp_subscriberfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));
    DIE(rc < 0, "setsockopt failed");

    fds.push_back(pollfd{tcp_subscriberfd, POLLIN, 0});
    nfds++;

    // create a new client
    tcp_client *new_client = new tcp_client();
    new_client->sockfd = tcp_subscriberfd;
    new_client->is_connected = true;
    strcpy(new_client->ip, inet_ntoa(tcp_subscriber_addr.sin_addr));
    new_client->port = ntohs(tcp_subscriber_addr.sin_port);
    strcpy(new_client->id, ""); // the ID is not known yet

    // add the new client to the set of subscribers
    subscribers.insert(new_client);
}

void exit_server(std::vector<struct pollfd> fds, int nfds) {
    char buffer[MAX_BUFFER_LEN];
    fgets(buffer, MAX_BUFFER_LEN, stdin);

    // check if it is a valid command
    if (strcmp(buffer, "exit") != 0) {
        return;
    }

    // close all the file descriptors
    for (int i = 0; i < nfds; i++) {
        close(fds[i].fd);
    }

    // free the memory allocated for the clients
    for (auto subscriber : subscribers) {
        delete subscriber;
    }

    // erase the set of subscribers
    subscribers.clear();
}

tcp_client* client_exists(char id[]) {
    // check if the client is already in the set of subscribers
    for (auto subscriber : subscribers) {
        if (strcmp(subscriber->id, id) == 0) {
            return subscriber;
        }
    }
    return NULL;
}

void handle_connection(tcp_message msg, std::vector<struct pollfd> &fds, int &nfds, int index) {
    int rc;
    // the client is connecting to the server
    // check if the client is already in the set of subscribers
    tcp_client *subscriber = client_exists(msg.id);
    if (subscriber) {
        // the client is already in the set of subscribers
        if (!subscriber->is_connected) {
            // the client is not connected
            // update the client
            subscriber->is_connected = true;
            subscriber->sockfd = fds[index].fd;

            // the new ip and the new port are from the new connection
            struct sockaddr_in tcp_subscriber_addr;
            socklen_t socket_len = sizeof(struct sockaddr_in);
            memset(&tcp_subscriber_addr, 0, socket_len);
            rc = getpeername(fds[index].fd, (struct sockaddr *)&tcp_subscriber_addr, &socket_len);
            DIE(rc < 0, "getpeername failed");

            strcpy(subscriber->ip, inet_ntoa(tcp_subscriber_addr.sin_addr));
            subscriber->port = ntohs(tcp_subscriber_addr.sin_port);

            // erase the entry of the last connection in the set, it is not needed anymore
            for (auto subscriber : subscribers) {
                // the last connection is the one with the empty ID
                if (strcmp(subscriber->id, "") == 0) {
                    subscribers.erase(subscriber);
                    break;
                }
            }

            // erase the subscriber from the set of subscribers
            subscribers.erase(subscriber);

            // add the updated subscriber to the set of subscribers
            subscribers.insert(subscriber);

            printf("New client %s connected from %s:%d.\n", subscriber->id,
                    subscriber->ip, subscriber->port);
            fflush(stdout);
        } else {
            // the client is already connected
            printf("Client %s already connected.\n", subscriber->id);
            fflush(stdout);

            // shutdown the connection
            rc = shutdown(fds[index].fd, SHUT_RDWR);
            DIE(rc < 0, "shutdown failed");

            // reject the connection, remove the file descriptor from the pollfd structure
            rc = close(fds[index].fd);
            DIE(rc < 0, "close failed");

            // remove the file descriptor from the pollfd structure
            fds.erase(fds.begin() + index);
            nfds--;

            // remove the client from the set of subscribers
            subscribers.erase(subscriber);
        }
    } else {
        // the client is not in the set of subscribers, so add it
        for (auto subscriber : subscribers) {
            if (strcmp(subscriber->id, "") == 0) {
                strcpy(subscriber->id, msg.id);
                printf("New client %s connected from %s:%d.\n",
                    subscriber->id, subscriber->ip, subscriber->port);
                fflush(stdout);
            }
            break;
        }
    }
}

void handle_subscribe(tcp_message msg, std::vector<struct pollfd> &fds, int &nfds, int index) {
    // the client is subscribing to a topic
    // get the client
    tcp_client *subscriber = client_exists(msg.id);

    // get the topic from the message
    std :: string topic(msg.topic);

    // check if the topic is already in the map
    if (topics.find(topic) == topics.end()) {
        // the topic is not in the map
        // create a new vector of clients for the topic
        std :: vector<tcp_client *> clients;
        clients.push_back(subscriber);

        // add the topic to the map
        topics.insert({topic, clients});
    } else {
        // the topic is in the map
        // add the client to the vector of clients for the topic
        topics[topic].push_back(subscriber);
    }
}

void handle_unsubscribe(tcp_message msg, std::vector<struct pollfd> &fds, int &nfds, int index) {
    // the client is unsubscribing from a topic
    // get the client
    tcp_client *subscriber = client_exists(msg.id);

    // get the topic from the message
    std :: string topic(msg.topic);

    // check if the topic is in the map
    if (topics.find(topic) != topics.end()) {
        // the topic is in the map
        // remove the client from the vector of clients for the topic
        for (long unsigned int i = 0; i < topics[topic].size(); i++) {
            if (strcmp(topics[topic][i]->id, subscriber->id) == 0) {
                topics[topic].erase(topics[topic].begin() + i);
                break;
            }
        }
    }
}

void handle_udp_message(int udpfd, std::vector<struct pollfd> fds, int &nfds) {
    // receive a message from a udp client
    char buffer[MAX_BUFFER_LEN];

    struct sockaddr_in udp_client_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    memset(&udp_client_addr, 0, socket_len);

    // extract the message from the udp client
    int rc = recvfrom(udpfd, buffer, MAX_BUFFER_LEN, 0, (struct sockaddr *)&udp_client_addr, &socket_len);
    DIE(rc < 0, "recvfrom failed");

    // add the null terminator
    buffer[rc] = '\0';

    // build the udp_message from the buffer
    udp_message msg = build_udp_message(buffer);

    strcpy(msg.ip, inet_ntoa(udp_client_addr.sin_addr));
    msg.port = ntohs(udp_client_addr.sin_port);

    std :: string topic(msg.topic);

    std :: unordered_set<std :: string> ids_sent;

    // check every topic in the map, to see if the topic from the udp message matches the topic
    for (auto topic_entry : topics) {
        if (match_topic(topic, topic_entry.first)) {
            // the topic from the udp message matches the topic from the map
            // send the message to every client that is subscribed to the topic
            for (auto subscriber : topic_entry.second) {
                // check if the message was already sent to the client
                if (ids_sent.find(subscriber->id) == ids_sent.end()) {
                    // the message was not sent to the client
                    // add the client to the set of clients that received the message
                    ids_sent.insert(subscriber->id);

                    // check if the client is connected
                    if (subscriber->is_connected) {
                        // the client is connected
                        // send the message to the client
                        rc = send_all(subscriber->sockfd, &msg, sizeof(udp_message));
                        DIE(rc < 0, "send_all failed");
                    }
                }
            }
        }
    }

    // delete the map
    ids_sent.clear();
}

void  handle_tcp_exit(char id[], std :: vector<struct pollfd> &fds, int &nfds, int index) {
    int rc;
    // the client is disconnecting from the server
    tcp_client *subscriber = client_exists(id);

    printf("Client %s disconnected.\n", subscriber->id);
    fflush(stdout);

    // mark the client as disconnected
    subscriber->is_connected = false;

    // remove the file descriptor from the pollfd structure
    rc = close(fds[index].fd);
    DIE(rc < 0, "close failed");

    // remove the file descriptor from the pollfd structure
    fds.erase(fds.begin() + index);
    nfds--;

    // remove the client from the set of subscribers
    subscribers.erase(subscriber);

    // add the updated client to the set of subscribers
    subscribers.insert(subscriber);
}

void handle_tcp_request(std :: vector<struct pollfd> &fds, int &nfds, int index) {
    // receive a request from a tcp client/subscriber
    tcp_message msg;
    int rc = recv_all(fds[index].fd, &msg, sizeof(tcp_message));
    DIE(rc < 0, "recv_all failed");


    // check what kind of command is
    switch (msg.cmd) {
        case SERVER_CONNECT: {
            handle_connection(msg, fds, nfds, index);
            break;
        }
        case SUBSCRIBE: {
            // the client is subscribing to a topic
            handle_subscribe(msg, fds, nfds, index);
            break;
        }
        case UNSUBSCRIBE: {
            // the client is unsubscribing from a topic
            handle_unsubscribe(msg, fds, nfds, index);
            break;
        }
        case EXIT: {
            // the client is disconnecting from the server
            handle_tcp_exit(msg.id, fds, nfds, index);

            break;
        }
        default: 
            break;
    }
}

void run_server(int listendfd, int udpfd, int server_port) {
    // create the pollfd structure
    std :: vector <struct pollfd> fds;
    int nfds = 3; // the first three file descriptors are the listenfd, STDIN_FILENO and udpfd

    fds.push_back(pollfd{listendfd, POLLIN, 0});
    fds.push_back(pollfd{STDIN_FILENO, POLLIN, 0});
    fds.push_back(pollfd{udpfd, POLLIN, 0});

    while (1) {
        // poll the file descriptors
        int rc = poll(fds.data(), nfds, -1);
        DIE(rc < 0, "poll failed");
        
        // see which file descriptor occured an event
        for (int i = 0; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                switch (i) {
                    case 0: {
                        // it is the listenfd
                        // accept the incoming connection from a TCP subscriber
                        connect_to_server(listendfd, fds, nfds);
                        break;
                    } 
                    case 1: {
                        // read the command
                        // the only command is exit so it will close the server
                        exit_server(fds, nfds);
                        return;
                    }
                    case 2: {
                        // receives a message from udp client and must forward to the subscriebers
                        handle_udp_message(udpfd, fds, nfds);
                        break;
                    }
                    // otherwise
                    default: {
                        // receives a request from a subscriber
                        handle_tcp_request(fds, nfds, i);
                        break;
                    }
                }   
            }
        }
    }
}

int main(int argc, char *argv[]) {
    // most of the following code is similar to the one in subscriber.cpp (lab 07)
    if (argc != 2) {
        printf("\n Usage: %s <port>\n", argv[0]);
        return 1;
    }

    // decativate the bufffering for the stdout
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    // treat the port as a number
    uint16_t port;
    int rc = sscanf(argv[1], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    // create the listen socket for capturing the incoming connections
    const int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(listenfd < 0, "creating a socket failed");

    // create the udp socket for receiving the messages from the udp clients
    const int udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udpfd < 0, "creating a socket failed");

    // complete the sockaddr_in structure with the necessary data
    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY; // accept any incoming address

    // bind the socket to the address and port
    rc = bind(listenfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "bind failed");

    // bind the socket from the udp clients
    rc = bind(udpfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "bind failed");

    // listen for incoming connections
    rc = listen(listenfd, MAX_CONNECTIONS);
    DIE(rc < 0, "listen failed");

    // run the server
    run_server(listenfd, udpfd, port);

    // close the listen socket
    close(listenfd);

    return 0;
}