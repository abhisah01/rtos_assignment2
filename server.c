#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <pthread.h>

#define MAX_CLIENTS 10 // limiting to only 10 clients to connect simultaneously
#define BUF_SIZE 2048

int uid = 1;
unsigned int cli_count = 0; // client count

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER; // mutex for the thread

// client structure
typedef struct {
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char name[32];
} client_t;

client_t *clients[MAX_CLIENTS] = {NULL}; // array of connected clients

// displaying all the connected users
void displayUsers() {
	pthread_mutex_lock(&clients_mutex);

	printf("\nupdated list of connected users...\n");
	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(clients[i] != NULL) {
			printf("%s\n", clients[i]->name);
		}
	}

	pthread_mutex_unlock(&clients_mutex);
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

// add clients to queue
void queue_add(client_t *cli){
	pthread_mutex_lock(&clients_mutex);

	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(!clients[i]) {
			clients[i] = cli;
			break;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

// remove clients from the queue
void queue_remove(int uid) {
	pthread_mutex_lock(&clients_mutex);

	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(clients[i]) {
			if(clients[i]->uid == uid){
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

// send messages to all clients except the sender
void send_message(char *s, int uid) {
	pthread_mutex_lock(&clients_mutex);

	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(clients[i]) {
			if(clients[i]->uid != uid) {
				if(write(clients[i]->sockfd, s, strlen(s)) < 0){
					perror("Error");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

// handle all communication with the client
void *handle_client(void *arg) {
	char buff_out[BUF_SIZE];
	char name[32];
	int leave_flag = 0;

	cli_count++;
	client_t *cli = (client_t*) arg;

	// name of the connected client will be printed
	if(recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) < 2 || strlen(name) >= 32-1) {
		printf("you didn't enter the name...\n");
		leave_flag = 1;
	} else{
		strcpy(cli->name, name);
		sprintf(buff_out, "%s has joined...\n", cli->name);

		// un-comment it and see the background work
		// printf("%s", buff_out); // printing on the server side

		send_message(buff_out, cli->uid); // sending to all the previously connected clients

		// display the names of the connected users
		displayUsers();
	}

	bzero(buff_out, BUF_SIZE);

	while(1) {
		if(leave_flag)
			break;

		int receive = recv(cli->sockfd, buff_out, BUF_SIZE, 0);
		if (receive > 0){
			if(strlen(buff_out) > 0){
				send_message(buff_out, cli->uid);
			}
		} else if (receive == 0 || strcmp(buff_out, "exit") == 0){
			sprintf(buff_out, "%s has left the chat...\n", cli->name);
			send_message(buff_out, cli->uid);
			leave_flag = 1;
		} else {
			sprintf(buff_out, "%s has left the chat due to some technical reasons...\n", cli->name);
			send_message(buff_out, cli->uid);
			leave_flag = 1;
		}

		bzero(buff_out, BUF_SIZE);
	}

    // delete client from queue and yield the thread
	close(cli->sockfd);
	queue_remove(cli->uid);
	free(cli);
	cli_count--;

	// display the names of the connected users
	displayUsers();

	pthread_detach(pthread_self());

	return NULL;
}

int main(int argc, char *argv[]) {
	if(argc != 2) {
		printf("Usage :: ./server <port>\n");
		return EXIT_FAILURE;
	}

	struct sockaddr_in server, client;
	int socket_desc, client_desc, port_no;
	socklen_t size_client;
	pthread_t threadID;

	socket_desc = socket(AF_INET, SOCK_STREAM, 0); // creating socket at server Side
	if(socket_desc == -1) {
		perror("Error");
		exit(0);
	}

	memset((void*) &server, 0, sizeof(server)); // clearing the server structure before initialization
	port_no = atoi(argv[1]);

	// socket address structure initialization
	server.sin_family = AF_INET; // setting domain (IPv4)
	server.sin_addr.s_addr = INADDR_ANY; // permits any incoming IP (we can customize this)
	server.sin_port = htons(port_no); // setting port number form command line arguments

	if(bind(socket_desc,(struct sockaddr*) &server, sizeof(server)) != 0) { // bind the socket address structure to the socket
		perror("Error");
		exit(0);
	}

	if(listen(socket_desc, 10) != 0) {
		perror("Error");
		exit(0);
	}

	printf("\n========= Welcome to VoIP based Walkie Talkie =========\n");

	while(1) {
		memset((void*) &client, 0, sizeof(client)); // clearing the client structure before initialization

	    if((client_desc = accept(socket_desc, (struct sockaddr*) &client, &size_client)) < 0) { // accepting the client request
	        perror("Error");
	        exit(0);
	    }

		// assign client settings
		client_t *cli = (client_t*) malloc(sizeof(client_t));
		cli->address = client;
		cli->sockfd = client_desc;
		cli->uid = uid++;

		// add client to the queue
	    queue_add(cli);

	    // forking the thread
	    pthread_create(&threadID, NULL, &handle_client, (void*) cli);
	}

	return 0;
}