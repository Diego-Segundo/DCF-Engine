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
#define PORT "8080"
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
int get_listener_socket(void)
{
    struct addrinfo serverInfo, *ptrToServerInfo, *ptrTmpCopy; // this copy is used to free 'ptrToServerInfo
    int listeningSock = 0, result = 0, yes = 1;

    memset(&serverInfo, 0, sizeof(serverInfo));
    serverInfo.ai_family = AF_INET;
    serverInfo.ai_socktype = SOCK_STREAM;
    serverInfo.ai_flags = AI_PASSIVE;

    if ((result = getaddrinfo(NULL, PORT, &serverInfo, &ptrToServerInfo)) < 0){
        fprintf(stderr, "ERROR: %s", gai_strerror(result));
        exit(EXIT_FAILURE);
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
    printf("server: started listening on: %s:%s\n", inet_ntop(ptrTmpCopy->ai_family,ptrTmpCopy->ai_addr, serverAddrBuff, INET6_ADDRSTRLEN),PORT);

    freeaddrinfo(ptrToServerInfo);

    if ((listen(listeningSock, 10)) < 0){
        return -1;
    }
    return  listeningSock;

}
void setup_incomming_connection(struct pollfd *pfds[], int listeningSock, int *fd_count, int *fd_size, int idx)
{
    int newfd = 0; 
    struct sockaddr_storage client_addr;
    char ipaddr[INET6_ADDRSTRLEN]; //ip info buff
    socklen_t addrlen;
    memset(ipaddr, 0, INET6_ADDRSTRLEN);
    
    
    addrlen = sizeof(client_addr);
    if ((newfd = accept(listeningSock, (struct sockaddr *)&client_addr, &addrlen)) < 0){
        perror("accept");
    } else {
            add_to_pfds(pfds, newfd, fd_count, fd_size);
            printf("server: new connection from: %s on socket: %d\n", inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), ipaddr, INET6_ADDRSTRLEN), newfd);
           } 
}

int read_incomming_date(struct pollfd pfds[], char buff[], int *fd_count, int i)
{
                    //Its a Client sending data
                    int sender_fd = pfds[i].fd; 
                    int nbytes = 0, nRead = 0, message_length = 0;
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
                        buff[nbytes] = '\0';
                        memset(buff, 0, 1024);
                        while(nRead < message_length && nRead < 1024){
                             if ((nbytes = recv(sender_fd, buff + nRead, 1024 - nRead, 0)) <= 0){
                                if (nbytes == 0){
                                // Connection closed
                                printf("server: Client with socket: %d hung up\n", sender_fd);
                            }else{
                                perror("revc");
                            }
                            break;
                        }
                            nRead += nbytes;
                        }
                        buff[nRead] = '\0';
                        nRead = 0;
                    }
                    return message_length;
}

// Server start-up
int main(int argc, char *argv[])
{
    // func declaration for linker phase
    void *get_in_addr(struct sockaddr *sa);
    void add_to_pfds(struct pollfd **pfds, int new_fd, int *fd_count, int *fd_size);
    void del_from_pfds(struct pollfd pfds[], int *fd_count, int idx);
    int get_listener_socket();
    void setup_incomming_connection(struct pollfd *pfds[], int listeningSock, int *fd_count, int *fd_size, int idx);
    int read_incomming_date(struct pollfd pfds[], char buff[], int *fd_count, int idx);
    
    int listeningSock, newfd, sender_fd, fd_count, fd_size; 
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
    
    listeningSock = get_listener_socket();
    if (listeningSock < 0){
        perror("listeningSock");
        exit(EXIT_FAILURE);
    }
    add_to_pfds(&pfds,listeningSock, &fd_count, &fd_size);

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
                    setup_incomming_connection(&pfds, listeningSock, &fd_count, &fd_size, i);
                } else{
                    int message_length = read_incomming_date(pfds, buff, &fd_count, i);
                    sender_fd = pfds[i].fd; 
                    for (int j = 0; j < fd_count; j++){
                        if (pfds[j].fd >= 0 && pfds[j].fd != listeningSock && pfds[j].fd != sender_fd){
                            if (send(pfds[j].fd, buff, message_length + 1, 0) < 0){
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