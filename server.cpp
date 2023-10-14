#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <mutex>
#include <unistd.h>
#include <sys/ipc.h>
#include <string.h>
#define STRINGIFY(x) #x


const int MAXCLIENT = 10;
struct clientInfo {
    int clientSocket; // handle
    sockaddr_in clientAddr; // Socket address, which consists of IP and port
    int status; 
};
struct clientInfo clientList[MAXCLIENT];
int clientIndex = 0;

/* Set datapack */
enum msgType {CONNECT = 1, DISCONNECT, GET_TIME, GET_NAME, GET_CLIENT_LIST, SEND_MSG, REPOST};

const int MAX_SIZE = 8 * 256;

struct Message {
    long type;
    char data[MAX_SIZE - 2];
};

void *handle_client(void* client_socket){
    int clientSocket = *(int*)client_socket;
    char buffer[] = "Hello, I am server";
    Message bin;
    for(int i=0;i<strlen(buffer);i++) bin.data[i] = buffer[i];
    bin.type = CONNECT;
    send(clientSocket, &bin, sizeof(bin), 0);
    struct Message msg;
    memset(&msg, 0, sizeof(msg));
    while (true) {
        /* https://www.ibm.com/docs/en/zos/2.4.0?topic=functions-recv-receive-data-socket*/
        int recv_len = recv(clientSocket, &msg, sizeof(msg), 0);
        printf("%d\n", recv_len);
        if (recv_len < 0) {
            printf("Fail to receive message from client %d, error code: %d\n", clientSocket, errno);
            continue;
        } else if (recv_len == 0) {
            printf("Client %d close connection!\n", clientSocket);
            break;
        }
        printf("+----------------------------------------------------------+\n");
        printf("Receive Data Package from client: %d\n", clientSocket);

        if(msg.type == DISCONNECT) { // Loop exit
            printf("Type:DISCONECT\n");
            close(clientSocket);
            printf("Client %d disconnected\n", clientSocket);
            for (int i = 0; i < clientIndex; i++) {
                if (clientList[i].clientSocket == clientSocket) {
                    clientList[i].status = 0;
                    /* Remove the client from the list*/
                    for (int j = i; j < clientIndex - 1; j++) {
                        clientList[j] = clientList[j + 1];
                    }
                }
            }
            printf("Success to remove client %d from list\n", clientSocket);
            break;
        } else if (msg.type == GET_TIME) {
            printf("Type:GET_TIME\n");
            time_t t;
            time(&t);
            struct Message msg_ret;
            sprintf(msg_ret.data, "%ld", t);
            msg_ret.type = GET_TIME;
            if (send(clientSocket, &msg_ret, sizeof(msg_ret), 0) < 0) {
                printf("Fail to send message to client %d, error code: %d\n", clientSocket, errno);
            } else {
                printf("Success to send time %s to client %d\n",ctime(&t), clientSocket);
            }

        printf("+----------------------------------------------------------+\n");

        } else if (msg.type == GET_NAME) {
            printf("Type:GET_NAME\n");
            Message msg_ret;
            gethostname(msg_ret.data, sizeof(msg_ret.data));
            msg_ret.type = GET_NAME;
            if (send(clientSocket, &msg_ret, sizeof(msg_ret), 0) < 0) {
                printf("Fail to send message to client %d, error code: %d\n", clientSocket, errno);
            } else {
                printf("Success to send name %s to client %d\n", msg_ret.data, clientSocket);
            }
        printf("+----------------------------------------------------------+\n");

        } else if (msg.type == GET_CLIENT_LIST) {
            printf("Type:GET_CLIENT_LIST\n");
            Message msg_ret;
            char buffer[MAX_SIZE-2];
            sprintf(buffer, "Client list: \n");
            for (int i = 0; i < clientIndex; i++) {
                sprintf(buffer, "%s%d: address: %s:%d\n","Client",i + 1, inet_ntoa(clientList[i].clientAddr.sin_addr), ntohs(clientList[i].clientAddr.sin_port)); // Question
            }
            sprintf(msg_ret.data, "%s", buffer); // Copy buffer to msg_ret.data
            msg_ret.type = GET_CLIENT_LIST;
            if (send(clientSocket, &msg_ret, sizeof(msg_ret), 0) < 0) {
                printf("Fail to send message to client %d, error code: %d\n", clientSocket, errno);
            } else {
                printf("Success to send client list to client %d\n", clientSocket);
            }
            printf("+----------------------------------------------------------+\n");
        } else if (msg.type == SEND_MSG) {
            printf("Type:SEND_MSG\n");
            in_port_t port;
            char content[MAX_SIZE - 50];
            int number;
            memset(content, 0, sizeof(content));
            sscanf(msg.data, "%d:%s",  &number, content);
            number--;
            struct in_addr addr;
            addr.s_addr = clientList[number].clientAddr.sin_addr.s_addr;
            port = clientList[number].clientAddr.sin_port;
            printf("Client send ip:%s, port:%u, content:%s\n", inet_ntoa(addr), port, content);
            int targetSocket;
            if(clientIndex < number && number >= 0){
                targetSocket = -1;
            }
            else{
                targetSocket = number;
            }
            Message msg_ret;
            msg_ret.type = SEND_MSG;
            if (targetSocket == -1) {
                printf("Fail! Cliend sending address is not connected!\n");
                sprintf(msg_ret.data, "Fail to send message to client %s:%u\n", inet_ntoa(addr), port);
            } else {
                Message msg_send;
                msg_send.type = REPOST; 
                sprintf(msg_send.data, "%s", content);
                sprintf(msg_ret.data, "%s", "Forward Success!\n");
                if (send(clientList[targetSocket].clientSocket, &msg_send, sizeof(msg_send), 0) < 0) {
                    printf("Fail to Forward message to client %d, error code: %d\n", clientList[targetSocket].clientSocket, errno);
                } else {
                    printf("Success to Forward message to client %d\n", clientSocket);
                }
            }
            if (send(clientSocket, &msg_ret, sizeof(msg_ret), 0) < 0) {
                printf("Fail to send message to client %d, error code: %d\n", clientSocket, errno);
            } else {
                printf("Success to return success forward signal to  %d\n", clientSocket);
            }
            printf("+----------------------------------------------------------+\n");
        } else {
            printf("Unknown message type %ld\n", msg.type);
            printf("+----------------------------------------------------------+\n");
        }
        memset(&msg, 0, sizeof(msg));
    }
    return NULL;
}

int main() {
    /*
    * socket()的参数含义如下:
    * AF_INET - 使用IPv4协议
    * SOCK_STREAM - 使用面向连接的TCP协议
    * 0 - 使用默认的网络协议(IPPROTO_IP for AF_INET)
    * 所以这里用socket()函数创建了一个使用IPv4和TCP协议的socket
    */
    /* SocketFD file descriptor*/
    int socketDestination = socket(AF_INET, SOCK_STREAM, 0);
    /* Set server listen address*/
    if (socketDestination == -1) {
        printf("Fail to create socket\n");
        exit(EXIT_FAILURE); 
    } else {
        printf("Success to create socket\n");
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(1733); // 使用学号后4位作为端口
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    /* Bind the address to socket*/
    bind(socketDestination, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    printf("Success to bind address to socket\n");
    listen(socketDestination, MAXCLIENT);
    printf("Success to listen socket\n");


    while(true) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        /*
        It extracts the first connection request 
        on the queue of pending connections for 
        the listening socket, sockfd, creates a 
        new connected socket, 
        and returns a new file descriptor referring to that socket
        */
        printf("Waiting for client connection...\n");
        int clientSocket = accept(socketDestination, (struct sockaddr*)&clientAddr, &clientAddrLen);
        clientList[clientIndex].clientSocket = clientSocket;
        clientList[clientIndex].clientAddr = clientAddr;
        clientList[clientIndex++].status = 1;
        printf("Success to accept client connection handle: %d, address: %s, port: %d\n", clientSocket, inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

        pthread_t conn_thread;
        pthread_create(&conn_thread, NULL, handle_client, &clientSocket);
    }
}