#include "common.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <iostream>
#include <stdio.h>
#include <arpa/inet.h>

int recv_all(int sockfd, void *buffer, size_t len) {

    size_t bytes_received = 0;
    size_t bytes_remaining = len;
    char *buff = (char *) buffer;

    while (bytes_remaining) {
        // receive bytes
        int bytes = recv(sockfd, buff, bytes_remaining, 0);
        if(bytes < 0) {
            return -1;
        }
        if (bytes == 0) {
            break;
        }
        bytes_received += bytes;
        bytes_remaining -= bytes;
        buff += bytes;
    }

    return bytes_received;
}

int send_all(int sockfd, void *buffer, size_t len) {
    size_t bytes_sent = 0;
    size_t bytes_remaining = len;
    char *buff = (char *) buffer;
    while(bytes_remaining) {
        // send bytes
        int bytes = send(sockfd, buff, bytes_remaining, 0);
        if(bytes < 0) {
            return -1;
        }
        bytes_sent += bytes;
        bytes_remaining -= bytes;
        buff += bytes;
    }

    return bytes_sent;
}

udp_message build_udp_message(char *buffer) {
    udp_message msg;
    memset(&msg, 0, sizeof(udp_message));

    // copy first the topic
    memcpy(msg.topic, buffer, MAX_TOPIC_LEN - 1);
    buffer += MAX_TOPIC_LEN - 1;
    

    // copy the data type
    msg.data_type = *buffer;
    buffer += sizeof(u_int8_t);

    // check the data type
    switch (msg.data_type) {
        case INT: {
            // get the sign byte
            int8_t sign = *buffer;
            buffer += sizeof(int8_t);

            // get the integer
            int32_t *data = (int32_t *) buffer;
            int64_t number = ntohl(*data); // convert to host byte order

            // check the sign
            if (sign == 1) {
                number = -number;
            }

            // copy in the message content the number
            memcpy(msg.content, &number, sizeof(int64_t));

            break;
        }
        case SHORT_REAL: {
            // get the short real
            int16_t *data = (int16_t *) buffer;
            float number = ntohs(*data) / (float)100; // convert to host byte order

            // copy in the message content the number
            memcpy(msg.content, &number, sizeof(float));
            break;
        }
        case FLOAT: {
            // get the sign byte
            int8_t sign = *buffer;
            buffer += sizeof(int8_t);

            // get the number
            uint32_t *data = (uint32_t *) buffer;
            float number = ntohl(*data); // convert to host byte order
            buffer += sizeof(uint32_t);

            // get the power
            int8_t power = *buffer;

            // check the sign
            if (sign == 1) {
                number = -number;
            }

            // divide the number by 10^power
            number /= (float) std :: pow(10, power);

            // extract the numbr with 4 decimals
            number = (int)(number * 10000) / (float)10000;

            // transform to string
            memcpy(msg.content, &number, sizeof(float));

            break;
        }
        case STRING: {
            char *string = buffer;
            strcpy(msg.content, string);
            break;
        }
    }

    return msg;
}

void print_udp_message(udp_message msg) {
    switch (msg.data_type) {
        case INT: {
            int64_t number;
            memcpy(&number, msg.content, sizeof(int64_t));
            printf("%s - INT - %ld\n", msg.topic, number);
            fflush(stdout);
            break;
        }
        case SHORT_REAL: {
            float number;
            memcpy(&number, msg.content, sizeof(float));
            printf("%s - SHORT_REAL - %.2f\n", msg.topic, number);
            fflush(stdout);
            break;
        }
        case FLOAT: {
            float number;
            memcpy(&number, msg.content, sizeof(float));
            printf("%s - FLOAT - %.4f\n", msg.topic, number);
            fflush(stdout);
            break;
        }
        case STRING: {
            printf("%s - STRING - %s\n", msg.topic, msg.content);
            fflush(stdout);
            break;
        }
    }
}

void get_command_arguments(char *buffer, int &arg_nr, char **command) {
    arg_nr = 0;

    char *token = strtok(buffer, " \n");
    
    while(token != NULL) {
        command[arg_nr++] = token;
        token = strtok(NULL, " \n");
    }
}

bool is_command_valid(int arg_nr, char **command) {
    if (strcmp(command[0], "subscribe") == 0) {
        if (arg_nr != 2) {
            printf("\nWrong usage of subscribe command\n");
            return false;
        }
        return true;
    } else if (strcmp(command[0], "unsubscribe") == 0) {
        if (arg_nr != 2) {
            printf("\nWrong usage of unsubscribe command\n");
            return false;
        }
        return true;
    } else if (strcmp(command[0], "exit") == 0) {
        if (arg_nr != 1) {
            printf("\nWrong usage of exit command\n");
            return false;
        }
        return true;
    } else {
        return false;
    }
}

bool match_topic(const std::string& topic, const std::string& pattern) {
    size_t topic_index = 0, pattern_index = 0;
    size_t l_topic = topic.length(), l_pattern = pattern.length();

    while (topic_index < l_topic && pattern_index < l_pattern) {
        if (pattern[pattern_index] == '*') {
            if (pattern_index + 1 == l_pattern)
                // it is at the end, it matches anything
                return true;

            pattern_index++; // move on to the next character
            for (size_t offset = 0; topic_index + offset <= l_topic; offset++) {
                // verify if the rest of the topic matches the rest of the pattern
                if (match_topic(topic.substr(topic_index + offset), pattern.substr(pattern_index))) {
                    return true;
                }
            }
            return false;
        } else if (pattern[pattern_index] == '+') {
            // if we are at the end of the pattern, we can match anythings
            if (topic_index == l_topic || topic[topic_index] == '/')
                return false;

            // go to the next '/'
            while (topic_index < l_topic && topic[topic_index] != '/')
                topic_index++;
            
            // go to the next character
            pattern_index++;
        } else {
            // if the characters are different, the pattern does not match
            if (pattern[pattern_index] != topic[topic_index])
                return false;
            topic_index++;
            pattern_index++;
        }
    }

    // verify if we reached the end of both the topic and the pattern
    return topic_index == l_topic && pattern_index == l_pattern;
}