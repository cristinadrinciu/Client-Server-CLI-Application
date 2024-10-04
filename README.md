# CLIENT SERVER CLIENT - SERVER UDP & TCP APPLICATION

### Drinciu Cristina 324CA

This program implements a client-server TCP and UDP for message management. TCP
clients can subscribe or unsubcribe to topics and the UDP clients send messages to the sub-
scribers through the server. The topics are many and the subscribtion can use wildcards. The UDP clients send the message to the subscribers that match the topic, handling the wildcards.

## Subscriber.cpp
* for this part I have used most of the code from lab07
* the program receives parametres such as client ID, IP and PORT from the command line.

### Connection Setup
* The client initializes a TCP connection to the server using the provided IP address and port number.
* Nagle's algorithm is disabled to ensure that messages are sent immediately without delay.

### Run Client

* Affter connect() to the server, the client sends a message to the server with its ID
* Create the vector of pollfd structures, that will contain 2 elements:
    * a file descriptor for the socket that makes possible the communication from the server
    * a file descriptor for the STDIN, where the client can receive commands from
* When an event occures on one of the file descriptors, execute the necessary opperation
    * if it receives a message from the server, it means that it receives an udp_message from the subscriber topic
        and the message is printed to the STDOUT
    * if it receives something from the STDIN it is a command which can be:
       - SUBSCRIBE: it sents a message to the server with the topic it wants to receive messages from now on
       - UNSUBSCRIBE: sents a message to the serber with the topic it wants to unsubscribe
       - EXIT: stops the connection to the server and stops the client
* It uses a structure for the tcp_messages for an easier job:
  *     struct tcp_message {
        command cmd;
        char id[MAX_ID_LEN];
        char ip[MAX_IP_LEN];
        uint16_t port;
        char topic[MAX_TOPIC_LEN];
        } __attribute__((packed));

## Server.cpp
* also for this part is used most of the code (in main function) from lab07

### Initialization
* initialize the server, create the sockets for listening other connections and for the udp_clipents
* bind the sockets
* listen for incoming connections

### Flow
* the server needs to stock some information about the subscribers and the topics they subscribed to, so we have 2 global variables:
    * subscribers: a set that stores the unique clients (structure tcp_client)
    * topics: a map that stores for each existent topic a vector of tcp_clients(subscribers)
* it is created the vector of pollfd structures with the existent file descriptors
    * the listen fd
    * the udp fd
    * the STDIN fd
    * the file descriptors for the future tcp clients
* depending on which file descriptor is triggered after the poll, it does the necessary operations:
    * listenfd: it mean that a new tcp client/subscriber wants to connect to the server, so accept the connection,
        add the new file descriptor to the vector and the new client to the set
    * STDIN: it only receives the exit command that closes the server and all the connections, freeing the resources
    * udp_fd: it receives a message (a buffer) from an udp_client, so:
      * it first builds the message in an udp_message structure for easier work
      * it check if the topic is in the map (if not, that means that there are no subscribers so the message is not forwarded)
      * check if the topic matches any of the keys in the map (cause the key topics may be wth wildcards)
      * forwards the message to the tcp_clients, also keeping evidence of the subscribers that already received the message,
        so that in case the topic matches multimple entries in the map and a subscriber may appear more than once,
            it won't receive the message twice
    * tcp_fds: it receives either the connect message(the id from the client) or a command:
        * CONNECT: it receives the id, so now check if the id already exists
        * if the client exists, check if it is offline, so make sure to reconnect it
        * if the client exists and it is online, refuse the connection
        * is the client does not exist, complete the id field from the structure in the set
        * SUBSCRIBE: add the client to the vector of the specified topic in the map, or if the topic is not there, add it as a key
        * UNSUBSCRIBE: remove the client from the specified topic from the map
        * EXIT: close the connection with the client and remove it from the fd vector, but from the set of clients, just mark it offline

## Common.cpp and Common.h
* it containts the DIE macro for deffensive programming
* the structures that are used in the program:
    * tcp_message
    * udp_message
    * tcp_client
* the useful functions that are used both in server.cpp and subscriber.cpp,recv_all and send_all that receives/sends the exact number of bits as given.
* the build_udp_message that extracts the topic, type and content of the message
* the print_udp_message that will print the mesage based on the existent types:
    * int
    * float
    * short_real
    * string
* the get_command_argumens that will build a vector with the argumens of a command
* is_commnd_valid will check is the command is valid
* match_topic works with two strings, the topic and the patter to see if the topic matches a pattern with or without wildcards (both * and +)
