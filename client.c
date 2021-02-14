#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#define BUF_SIZE 2048

char name[32];
int socket_desc;
int flag = 0;

void catch_ctrl_c_and_exit() {
    flag = 1;
}

// trimming the message to it's actual length
void str_trim_lf(char* arr, int length) {
    for(int i = 0; i < length; i++) {
        if(arr[i] == '\n') {
            arr[i] = '\0';
            break;
        }
    }
}

void send_msg_handler() {
    char message[BUF_SIZE];
    char buffer[BUF_SIZE + 32];

    bzero(message, BUF_SIZE);
    bzero(buffer, BUF_SIZE + 32);

    while(1) {
        fgets(message, BUF_SIZE, stdin);
        str_trim_lf(message, BUF_SIZE);

        if(strcmp(message, "exit") == 0) {
                break;
        } else {
            sprintf(buffer, "%s :: %s\n", name, message);
            send(socket_desc, buffer, strlen(buffer), 0);
        }

        bzero(message, BUF_SIZE);
        bzero(buffer, BUF_SIZE + 32);
    }

    catch_ctrl_c_and_exit();
}

void recv_msg_handler() {
    char message[BUF_SIZE];

    bzero(message, BUF_SIZE);

    while (1) {
        int receive = recv(socket_desc, message, BUF_SIZE, 0);
        if(receive > 0) {
            printf("%s", message);
        } else if(receive == 0) {
            break;
        }

        bzero(message, BUF_SIZE);
    }
}

int main(int argc, char *argv[]) {
    if(argc != 3){
        printf("Usage: ./client <IP address> <port>\n");
        return EXIT_FAILURE;
    }

    printf("Enter your name :: ");
    fgets(name, 32, stdin); // name input
    str_trim_lf(name, strlen(name)); // reducing to it's actual size

    if(strlen(name) > 32 || strlen(name) < 2){
        printf("name must be less than 30 and more than 2 characters...\n");
        return EXIT_FAILURE;
    }

	struct sockaddr_in server;
    int port_no;

    socket_desc = socket(AF_INET, SOCK_STREAM, 0); // creating the socket at client side
    if(socket_desc == -1) {
        perror("Error");
        exit(0);
    }

    memset(&server, 0, sizeof(server)); // clearing the server structure before initialization
    port_no = atoi(argv[2]);

    server.sin_family = AF_INET; // setting domain (IPv4)
    server.sin_port = htons(port_no); // setting port number from command line arguments

    if(inet_pton(AF_INET, argv[1], &server.sin_addr) <= 0) {
        printf("Invalid IP");
        exit(0);
    }

    if(connect(socket_desc, (struct sockaddr*) &server, sizeof(server)) != 0) { // connecting the client to the server
        perror("Error");
        exit(0);
    }

	send(socket_desc, name, 32, 0); // sending the name to server

    printf("you are now connected...\n\n");
    printf("========= Welcome to VoIP based Walkie Talkie =========\n\n");

    pthread_t send_msg_thread;
    if(pthread_create(&send_msg_thread, NULL, (void *) send_msg_handler, NULL) != 0) {
        perror("Error");
        return EXIT_FAILURE;
    }

    pthread_t recv_msg_thread;
    if(pthread_create(&recv_msg_thread, NULL, (void *) recv_msg_handler, NULL) != 0) {
        perror("Error");
        return EXIT_FAILURE;
    }

    while(1) {
        if(flag) {
            printf("\ngood bye...\n");
            break;
        }
    }

    close(socket_desc);

	return 0;
}