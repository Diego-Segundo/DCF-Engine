#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<unistd.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<poll.h>
#include<errno.h>
#define CPORT "8080"
#define SPORT "8081"
/* This file contains the server-setup, polling structure initialization, interface setup and managment with custom backend python service,
 client connection establishment/handalment, and DCF model utilization.*/

// Handler func to aid in the struc transaformation required to get IP addr.
// Only return void * bc inet_top() takes in a void *
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET){
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }
        return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}
// Handler func for adding file descriptors we want to monitor
void add_to_pfds(struct pollfd **pfds, int new_fd, int *fd_count, int *fd_size)
{

    if (*fd_count == *fd_size){
        *fd_size *= 2;
        struct pollfd *tmpPtr = realloc(*pfds, sizeof(struct pollfd) * (*fd_size));
        
        if (tmpPtr == NULL){
            perror("realloc");
            return;
        }
        *pfds = tmpPtr;
    }
   
    (*pfds)[*fd_count].fd = new_fd;
    (*pfds)[*fd_count].events = POLLIN;
    (*fd_count)++;
}

// handler func to override file descriptor we no longer want to monitor 
void del_from_pfds(struct pollfd pfds[], int *fd_count, int idx)
{
    pfds[idx] = pfds[*fd_count - 1];
    (*fd_count)--;
}

// Function responsible in the set-up and initialization of our socket
int get_listener_socket(char *port)
{
    struct addrinfo serverInfo, *ptrToServerInfo, *ptrTmpCopy; // this copy is used to free 'ptrToServerInfo
    int listeningSock = 0, result = 0, yes = 1;
    char *curr_port;

    memset(&serverInfo, 0, sizeof(serverInfo));
    serverInfo.ai_family = AF_INET;
    serverInfo.ai_socktype = SOCK_STREAM;
    serverInfo.ai_flags = AI_PASSIVE;

    if (strcmp(port, "Client")  == 0){
        curr_port = CPORT;
        if ((result = getaddrinfo(NULL, CPORT, &serverInfo, &ptrToServerInfo)) < 0){
            fprintf(stderr, "ERROR: %s", gai_strerror(result));
            exit(EXIT_FAILURE);
    }}else{
        curr_port = SPORT;
        if ((result = getaddrinfo(NULL, SPORT, &serverInfo, &ptrToServerInfo)) < 0){
            fprintf(stderr, "ERROR: %s", gai_strerror(result));
            exit(EXIT_FAILURE);
        }
    }


    for (ptrTmpCopy = ptrToServerInfo; ptrTmpCopy != NULL;  ptrTmpCopy = ptrTmpCopy->ai_next){
        if ((listeningSock = socket(ptrTmpCopy->ai_family, ptrTmpCopy->ai_socktype, ptrTmpCopy->ai_protocol)) < 0){
            perror("socket");
            continue;
        }
        if ((setsockopt(listeningSock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) < 0){
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }

        if ((bind(listeningSock, ptrTmpCopy->ai_addr, ptrTmpCopy->ai_addrlen)) < 0){
            perror("bind");
            continue;
        }
        break;
    }

    if (ptrTmpCopy == NULL){
        return -1;
    }

    char serverAddrBuff[INET6_ADDRSTRLEN];
    memset(serverAddrBuff, 0, sizeof(serverAddrBuff));
    printf("server: started listening on: %s:%s\n", inet_ntop(ptrTmpCopy->ai_family, get_in_addr(ptrTmpCopy->ai_addr), serverAddrBuff, INET6_ADDRSTRLEN),curr_port);

    freeaddrinfo(ptrToServerInfo);

    if ((listen(listeningSock, 10)) < 0){
        return -1;
    }
    return  listeningSock;

}
// Accept client request to connect to server or
// Accept python finance server request to connect to server
int setup_incomming_connection(struct pollfd *pfds[], int isServer, int listeningSock, int *fd_count, int *fd_size)
{
    int newfd = 0; 
    struct sockaddr_storage client_addr;
    char ipaddr[INET6_ADDRSTRLEN]; //ip info buff
    socklen_t addrlen;
    memset(ipaddr, 0, INET6_ADDRSTRLEN);
    
    
    addrlen = sizeof(client_addr);
    if ((newfd = accept(listeningSock, (struct sockaddr *)&client_addr, &addrlen)) < 0){
        perror("accept");
        return -1;
    } else {
        if (isServer){
            printf("server: new connection from: %s on socket: %d\n", inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), ipaddr, INET6_ADDRSTRLEN), newfd);
            return newfd;
        }else{
            add_to_pfds(pfds, newfd, fd_count, fd_size);
            printf("server: new connection from: %s on socket: %d\n", inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), ipaddr, INET6_ADDRSTRLEN), newfd);
        }
           } 
    return 0;
}
// Reads data from client and stores it in a heap allocated buffer
int read_incomming_data(struct pollfd pfds[], char buff[], int *fd_count, int i)
{
    //Its a Client sending data
    int sender_fd = pfds[i].fd; 
    int nbytes = 0, total_read = 0, message_length = 0;
    nbytes = recv(sender_fd, buff, 1024, 0); 
    if (nbytes <= 0){
        if (nbytes == 0){
            // Connection closed
            printf("server: Client with socket: %d hung up\n", sender_fd);
        }else{
            perror("revc");
        }
        del_from_pfds(pfds, fd_count, i);
        close(sender_fd);
        return 0;
    }else{
        message_length = atoi(buff);
        memset(buff, 0, 1024);
        while(total_read < message_length && total_read < 1024){
            if ((nbytes = recv(sender_fd, buff + total_read, 1024 - total_read, 0)) <= 0){
                if (nbytes == 0){
                // Connection closed
                printf("server: Client with socket: %d hung up\n", sender_fd);
                }else{
                    perror("revc");
                }
            del_from_pfds(pfds, fd_count, i);
            close(sender_fd);
            break;
        }
            total_read += nbytes;
        }
        buff[total_read] = '\0';
        message_length = total_read;
        total_read = 0;
    }
    return message_length;
}
// Request stock ticket closing price from python server
int req_stock_data(int server_fd, char buff[]){
    int process_and_store_financial_data(char buff[], int capacity);
    int data_read = 0, total_data_read = 0, capacity = 0;
        if ((send(server_fd, buff, strlen(buff) - 1 , 0)) < 0){
            perror("Msg not sent to server");
            return -1;
        } 
        if ((recv(server_fd, buff, 1024, 0)) < 0){
            perror("failed to read server data");
            return -1;
        }
        int msg_size = atoi(buff);
        memset(buff, 0, sizeof(*buff));
        while(total_data_read != msg_size){
            if ((data_read = recv(server_fd, buff + total_data_read, 1024 - total_data_read - 1, 0)) < 0){
                perror("failed to read server data");
                return -1;
            }
            total_data_read += data_read;
        }
        process_and_store_financial_data(buff, total_data_read);
        return msg_size;
}
int process_and_store_financial_data(char buff[], int capacity){
    double rev[4] = {0}, costOfRev[4] = {0}, OptExp[4] = {0}, depreciation[4] = {0}, taxRate[4] = {0}, capEx[4] = {0}, nwc[4] = {0}, number;
    int size = 0, done = 1;
    int buff_length = strlen(buff);
    char tmpBuff[buff_length];
    strcpy(tmpBuff, buff);
    int financial_component = 1; 
    char *endptr;
    char *token = strtok(tmpBuff, ","); 
    while (done == 1){
        switch (financial_component){
            case 1:
                if (*token == '#'){
                    financial_component += 1;
                    size = 0;
                    token = strtok(NULL, ",");
                    break;
                }
                number = 0;
                number = strtod(token, &endptr);
                rev[size] =  number;
                size++; 
                token = strtok(NULL, ",");
                break;
            case 2:
                if (*token == '#'){
                    financial_component += 1;
                    size = 0;
                    token = strtok(NULL, ",");
                    break;
                }
                number = 0;
                number = strtod(token, &endptr);
                costOfRev[size] = number;
                size++; 
                token = strtok(NULL, ",");
                break;
            case 3:
                if (*token == '#'){
                    financial_component += 1;
                    size = 0;
                    token = strtok(NULL, ",");
                    break;
                }
                number = 0;
                number = strtod(token, &endptr);
                OptExp[size] = number;
                size++; 
                token = strtok(NULL, ",");
                break;
            case 4:
                if (*token == '#'){
                    financial_component += 1;
                    size = 0;
                    token = strtok(NULL, ",");
                    break;
                }
                number = 0;
                number = strtod(token, &endptr);
                depreciation[size] = number;
                size++; 
                token = strtok(NULL, ",");
                break;
            case 5:
                if (*token == '#'){
                    financial_component += 1;
                    size = 0;
                    token = strtok(NULL, ",");
                    break;
                }
                number = 0;
                number = strtod(token, &endptr);
                taxRate[size] = number;
                size++; 
                token = strtok(NULL, ",");
                break;
            case 6:
                if (*token == '#'){
                    financial_component += 1;
                    size = 0;
                    token = strtok(NULL, ",");
                    break;
                }
                number = 0;
                number = strtod(token, &endptr);
                capEx[size] = number;
                size++; 
                token = strtok(NULL, ",");
                break;
            case 7:
                if (token == NULL || *token == '#'){
                    financial_component += 1;
                    size = 0;
                    token = strtok(NULL, ",");
                    break;
                }
                number = 0;
                number = strtod(token, &endptr);
                nwc[size] = number;
                size++; 
                token = strtok(NULL, ",");
                break;
            case 8:
                done = 0;
                break;
            default:
                done = 0;
                printf("Not a valid financial component\n");
                break;
        }
    }
    printf("Testing rev %lf\n", nwc[0]);
    return 0;

}

// Server start-up
int main(int argc, char *argv[])
{
    // func declaration for linker phase
    void *get_in_addr(struct sockaddr *sa);
    void add_to_pfds(struct pollfd **pfds, int new_fd, int *fd_count, int *fd_size);
    void del_from_pfds(struct pollfd pfds[], int *fd_count, int idx);
    int get_listener_socket(char *port);
    int setup_incomming_connection(struct pollfd *pfds[], int isServer, int listeningSock, int *fd_count, int *fd_size);
    int read_incomming_data(struct pollfd pfds[], char buff[], int *fd_count, int idx);
    int req_stock_data(int server_fd, char buff[]);
    
    int listeningSock, newfd, sender_fd, fd_count, fd_size, isServer = 0; 
    char *buff = calloc(1024, sizeof(char)); //msg buff

    memset(buff, 0, 1024);
   
    fd_count = 0;
    fd_size = 5;  
    struct pollfd *pfds = malloc(sizeof(struct pollfd) * fd_size);

    if (pfds == NULL){
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    memset(pfds, 0, sizeof(*pfds) * fd_size);
    
    int server_listening_socket = get_listener_socket("Server"); 
    if (server_listening_socket< 0){
        perror("Server socket failed");
        exit(EXIT_FAILURE);
    }
    listeningSock = get_listener_socket("Client");
    if (listeningSock < 0){
        perror("listeningSock");
        exit(EXIT_FAILURE);
    }
    add_to_pfds(&pfds,listeningSock, &fd_count, &fd_size);

    int server_socket =  setup_incomming_connection(&pfds, 1, server_listening_socket, &fd_count, &fd_size);
    while(1){
        
        int polled_events;// Continue checking for new events 
        // if (polled_events < 0){
        //         if (polled_events == -1 && errno == EINTR){
        //             printf("A signal interruped the sys call. Try again\n"); 
        //             continue;
        //         } else{
        //             perror("poll");
        //             exit(EXIT_FAILURE);
        //             break;
        //         }
        // } 
        do {
            int polled_events= poll(pfds, fd_count, -1); // Continue checking for new events
        } while (polled_events == -1 && errno == EINTR);
       
        for (int i = 0; i < fd_count; i++){
            memset(buff, 0, strlen(buff));
            if (pfds[i].revents & POLLIN){
                if (pfds[i].fd == listeningSock){
                    setup_incomming_connection(&pfds, isServer, listeningSock, &fd_count, &fd_size);
                } else{
                    int message_length = read_incomming_data(pfds, buff, &fd_count, i); 
                   
                    int result = req_stock_data(server_socket, buff);
                    if (result == -1){
                        perror("bad request");
                        continue;
                    }
                    message_length = result; 
                    printf("Stock X price: %s\n", buff);
                    //TODO: read python result and save it in a custom string
                    //TODO: start a subprocess with the necessary data received from the. Return the fair value stock price along witht the pfds array index. For quick message reponses to clients.
                    sender_fd = pfds[i].fd; 
                    for (int j = 0; j < fd_count; j++){
                        if (pfds[j].fd >= 0 && pfds[j].fd != listeningSock && pfds[j].fd != sender_fd){
                            if (send(pfds[j].fd, buff, message_length, 0) < 0){
                                perror("send");
                                fprintf(stderr, "invalid file descriptor %d\n", pfds[j].fd);
                            } 
                        } 
                    }

                }
            }
        }

    }
    free(buff);
    free(pfds);
        
    return 0;
}