#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h> 
#include "./messages/error_message.h"
#include "./messages/general_message.h"

void connection_handler(int socket_descriptor);
void main() {
    struct sockaddr_in server_address;
    struct sockaddr server;
    int socket_descripter,connection_descriptor,connect_status;

    socket_descripter = socket(AF_INET,SOCK_STREAM,0);
    if(socket_descripter == -1){
        perror(SOCKET_ERROR);
        _exit(0);
    }
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(4002);
    connect_status = connect(socket_descripter,(struct sockaddr *)&server_address,sizeof(server_address));
    if(connect_status == -1){
        perror(SERVER_CONNECTION_ERROR);
        close(connection_descriptor);
        _exit(0);
    }
    connection_handler(socket_descripter);
    close(socket_descripter);
}
void connection_handler(int socket_descriptor) {
    char read_buffer[1000],write_buffer[1000];
    ssize_t read_bytes,write_bytes;
    char temp_buffer[1000];
    do {
        bzero(read_buffer, sizeof(read_buffer));
        bzero(temp_buffer, sizeof(temp_buffer));
        read_bytes = read(socket_descriptor,read_buffer,sizeof(read_buffer));
        if(read_bytes == -1)
            printf(SERVER_READ_ERROR);
        else if (read_bytes == 0)
            printf(EMPTY_RESPONSE_ERROR);
        else if  (strchr(read_buffer, '^') != NULL){
            
            strncpy(temp_buffer, read_buffer, strlen(read_buffer) - 1);
            printf("%s\n", temp_buffer);
            write_bytes = write(socket_descriptor, "^", strlen("^"));
            if (write_bytes == -1)
            {
                perror(CLIENT_WRITE_ERROR);
                break;
            }
        }
        else if(strchr(read_buffer,'$')!=NULL) {
            strncpy(temp_buffer,read_buffer,strlen(read_buffer)-2);
            printf("%s\n",temp_buffer);
            printf(CLOSING_CONNECTION);
            break;
        }
        else{
            bzero(write_buffer,sizeof(write_buffer));
            if(strchr(read_buffer,'#')!=NULL)
                strcpy(write_buffer,getpass(read_buffer));
            else{
                printf("%s\n",read_buffer);
                scanf("%[^\n]%*c",write_buffer);
            }
            write_bytes = write(socket_descriptor,write_buffer,strlen(write_buffer));
            if(write_bytes == -1){
                perror(CLIENT_WRITE_ERROR);
                printf(CLOSING_CONNECTION);
                break;

            }
        }
     
    } while(read_bytes>0);
    close(socket_descriptor);
}