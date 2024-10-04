#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>
#include <unordered_map>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <string>

#define MAX_ID_LEN 10
#define MAX_TOPIC_LEN 51
#define MAX_CONTENT_LEN 1501
#define MAX_IP_LEN 16
#define MAX_CLIENTS 100
#define MAX_BUFFER_LEN 1555
#define MAX_NR_ARGUMENTS 4
#define MAX_ARG_LEN 1024

#define DIE(assertion, call_description)                                       \
  do {                                                                         \
    if (assertion) {                                                           \
      fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);                       \
      perror(call_description);                                                \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0)


int send_all(int sockfd, void *buff, size_t len);
int recv_all(int sockfd, void *buff, size_t len);

enum command {
    SUBSCRIBE = 1,
    UNSUBSCRIBE = 2,
    SERVER_CONNECT = 3,
    EXIT = 4
};

struct tcp_message {
    command cmd;
    char id[MAX_ID_LEN];
    char ip[MAX_IP_LEN];
    uint16_t port;
    char topic[MAX_TOPIC_LEN];
} __attribute__((packed));

struct udp_message {
    char ip[MAX_IP_LEN];
    uint16_t port;
    char topic[MAX_TOPIC_LEN];
    u_int8_t data_type;
    char content[MAX_CONTENT_LEN];
} __attribute__((packed));

// struct for tcp_client
struct tcp_client {
    int sockfd;
    bool is_connected;
    char id[MAX_ID_LEN];
    char ip[MAX_IP_LEN];
    uint16_t port;
};

enum type {
    INT = 0,
    SHORT_REAL = 1,
    FLOAT = 2,
    STRING = 3
};

udp_message build_udp_message(char *buffer);

void print_udp_message(udp_message msg);
void get_command_arguments(char *buffer, int &arg_nr, char *command[]);
bool is_command_valid(int arg_nr, char *command[]);
bool match_topic(const std::string& topic, const std::string& pattern);

#endif
