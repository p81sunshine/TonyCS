#include <iostream>

// #include <winsock2.h>
#include <stdio.h>
#include <sys/types.h>
// #include <windows.h>
#include <sys/ipc.h>
#include <arpa/inet.h>
#include <sys/msg.h>
#include <netinet/ip.h>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include<unistd.h>
#include<errno.h>

#include<pthread.h>
const int MAX_SIZE = 256;
enum type {CONNECT = 1, DISCONNECT, GET_TIME, GET_NAME, GET_CLIENT_LIST, SEND_MSG, REPOST};
void *helper_thread(void *arg);

struct Message {
    long type;
    char data[MAX_SIZE - 2];
};
using namespace std;
int msg_ID;

class client{
public:
    client();
    ~client();
    void start();
private:
    int client_sd;
    struct sockaddr_in server_addr;
    void connecting();
    void disconnecting();
    void gettime();
    void getname();
    void getlist();
    void sendmsg();
    void finish();
    pthread_t thrd;
};

client::client(){
    client_sd = -1;
    memset(&server_addr, 0, sizeof(server_addr));
    key_t key = ftok("c", 12);
    msg_ID = msgget(key, IPC_CREAT | 0666);
    if (msg_ID < 0){
        printf("[Error] Message queue create fail: %s\n", strerror(errno));
        return;
    }
}

client::~client(){
    close(client_sd);
}

void client::start(){
    cout << "+----------------------------------+\n";
    cout << "|   Welcome to use this client!    |\n";
    while(1){
        cout << "+----------------------------------+\n";
        cout << "| 1 : connect to server            |\n";
        cout << "| 2 : disconnect from server       |\n";
        cout << "| 3 : get time                     |\n";
        cout << "| 4 : get server name              |\n";
        cout << "| 5 : get client list              |\n";
        cout << "| 6 : send message                 |\n";
        cout << "| 7 : exit                         |\n";
        cout << "+----------------------------------+\n";
        cout << "please enter you choice:";
        int whichcase;
        cin >> whichcase;
        switch (whichcase){
        case 1:
            connecting();
            break;
        case 2:
            disconnecting();
            break;
        case 3:
            gettime();
            break;
        case 4:
            getname();
            break;
        case 5:
            getlist();
            break;
        case 6:
            sendmsg();
            break;
        case 7:
            finish();
            break;
        default:
            cout<<"error input"<<endl;
            break;
        }
}
    }


void client::connecting(){
    cout<<"connecting......"<<endl;
    //error detection
    if(client_sd!=-1){
        cout<<"Error:reconnect"<<endl;
        return ;
    }
    //get socket
    client_sd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sd < 0) {
        perror("Error: Create socket fail");
        return;
    }
    else{
        printf("socket success\n");
    }
    memset(&server_addr, 0, sizeof(server_addr));
    //data preparation
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1733);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    //connection
    int mark = connect(client_sd, (sockaddr *)&server_addr, sizeof(server_addr));
    if(mark < 0){
        printf("[Error] Connect fail, error: %s\n", strerror(errno));
        return;
    } else {
        printf("Connect success\n");
    }
    //create pthread
    if (pthread_create(&thrd, NULL, helper_thread, &client_sd) != 0)
    {
        printf("[Error] Create thread fails!\n");
    }
}

void client::disconnecting(){
    //error detection
    if(client_sd==-1){
        cout << "Error: No connect" << endl;
        return ;
    }
    //set type
    char info = DISCONNECT;
    //send the disconnect request
    int mark = send(client_sd, &info, sizeof(info), 0);
    if (mark < 0){
        printf("[Error] Disconnect send fail, error: %s\n", strerror(errno));
    }
    //cancel pthread
    pthread_cancel(thrd);
    close(client_sd);
    //set the state
    client_sd = -1;
    printf("[Info] Connection closed!\n");
}

void *helper_thread(void *arg)
{
    int sockfd = *(int *)arg;
    Message msg;
    while (1)
    {
        memset(&msg, 0, sizeof(msg));
        //receive the data
        if (recv(sockfd, &msg, sizeof(msg), 0) < 0){
            printf("[Error] helper recv fail, error: %s\n", strerror(errno));
        }
        //send to the main thread
        msgsnd(msg_ID, &msg, MAX_SIZE, 0);
    }
}

void client::gettime(){
    //error detection
    if (client_sd == -1){
        printf("[Error] No connection detected!\n");
        return;
    }
    //set type
    char data = GET_TIME;
    if (send(client_sd, &data, sizeof(data), 0) < 0)
    {
        printf("[Error] getServerTime send fail, error: %s\n", strerror(errno));
        return;
    }
    //create massage
    Message msg;
    long type = GET_TIME;
    //receive the message
    if (msgrcv(msg_ID, &msg, MAX_SIZE, type, 0) < 0)
    {
        printf("[Error] getServerTime recv fail, error: %s\n", strerror(errno));
        return;
    }
    //change the data into time
    time_t t;
    sscanf(msg.data, "%ld", &t);
    printf("Time: %s\n", ctime(&t));
    return;
}

void client::getname(){
    //error detection
    if (client_sd == -1){
        printf("[Error] No connection detected!\n");
        return;
    }
    //set type
    char data = GET_NAME;
    if (send(client_sd, &data, sizeof(data), 0) < 0)
    {
        printf("[Error] getServerTime send fail, error: %s\n", strerror(errno));
        return;
    }
    Message msg;
    long type = GET_NAME;
    //receive the message
    if (msgrcv(msg_ID, &msg, MAX_SIZE, type, 0) < 0)
    {
        printf("[Error] getServerTime recv fail, error: %s\n", strerror(errno));
        return;
    }
    //output the content
    printf("Server Name: %s\n", msg.data);
    return;
}

void client::getlist(){
    //error detection
    if (client_sd == -1){
        printf("[Error] No connection detected!\n");
        return;
    }
    //set the type
    char data = GET_CLIENT_LIST;
    //send data
    if (send(client_sd, &data, sizeof(data), 0) < 0)
    {
        printf("[Error] getServerTime send fail, error: %s\n", strerror(errno));
        return;
    }
    Message msg;
    long type = GET_CLIENT_LIST;
    //receive the data
    if (msgrcv(msg_ID, &msg, MAX_SIZE, type, 0) < 0)
    {
        printf("[Error] getServerTime recv fail, error: %s\n", strerror(errno));
        return;
    }
    //output the list
    printf("Client list: %s\n", msg.data);
    return;
}

void client::sendmsg(){
    //check connenction state
    if(client_sd == -1){
        cout<<"error: no connection"<<endl;
        return ;
    }
    //allow user input ip and port
    char newip[MAX_SIZE];
    int port;
    cout<<"please input IP:"<<endl;
    scanf("%s",newip);
    in_addr_t ip;
    ip = inet_addr(newip); // From string to in_addr_t
    cout<<"please input port"<<endl;
    scanf("%d",&port);
    Message msg;
    //set type
    msg.type = SEND_MSG;
    sprintf(msg.data, "%u:%d:", ip, port);
    char content[MAX_SIZE - 50];
    //read in the content
    cout<<"please input content"<<endl;
    scanf("%s", content);
    sprintf(msg.data + strlen(msg.data), "%s", content);
    printf("%s\n", msg.data);
    if (send(client_sd, &msg, sizeof(msg), 0) < 0)
    {
        printf("[Error] sendToOtherClient send fail, error: %s\n", strerror(errno));
        return;
    }
    memset(&msg, 0, sizeof(msg));
    long type = SEND_MSG;
    if (msgrcv(msg_ID, &msg, MAX_SIZE, type, 0) < 0)
    {
        printf("[Error] sendToOtherClient recv fail, error: %s\n", strerror(errno));
        return;
    }

    printf("Server Response: %s\n", msg.data);
    return;    
}

void client::finish(){
    //check the connection state
    if(client_sd!=-1){
        disconnecting();
    }
    exit(0);
}

int main(){
    client c;
    c.start();
    return 0;
}