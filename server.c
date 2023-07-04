#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h> 
#include <stdbool.h> 
#include <stdlib.h>  

#include "./messages/error_message.h"
#include "./messages/general_message.h"
#include "./utility/admin_utility.h"
#include "./utility/user_utility.h"
void connection_handler(int connection_descriptor);
void main() {

    struct sockaddr_in server_address, client_address;
    int socket_descripter,connection_descriptor,socket_bind_status,socket_listen_status,client_size;
    // getting socket 
    socket_descripter = socket(AF_INET,SOCK_STREAM,0);
    if(socket_descripter == -1){
        perror(SOCKET_ERROR);
        _exit(0);
    }
    // adding socket parameter
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(4002);
    // binding the socket address 
    socket_bind_status = bind(socket_descripter,(struct sockaddr *)(&server_address),sizeof(server_address));
    if(socket_bind_status == -1){
        perror(BIND_ERROR);
        _exit(0);
    }
    // listening the socket for connection 
    socket_listen_status = listen(socket_descripter,10);
    if(socket_listen_status == -1){
        perror(LISTEN_ERROR);
        _exit(0);
    }
    // getting connection 
    while(1){
        client_size = (int)sizeof(client_address);
        connection_descriptor = accept(socket_descripter,(struct sockaddr *)&client_address,&client_size);
        if(connection_descriptor==-1){
            perror(CLIENT_CONNECTION_ERROR);
            close(connection_descriptor);
        }
        else{
            if(!fork()){
            connection_handler(connection_descriptor);
            close(connection_descriptor);
            _exit(0);
        }
    }

}
close(socket_descripter);
}
/*
    param int connection_descriptor :- descriptor of the socket connection 
    functionality :- initial function to handle the connection invokes the welcome message and call corresponding.
    return :- None
*/
void connection_handler(int connection_descriptor) {
    printf("%s\n",CLIENT_CONNECT_SUCCESS);
    // read_buffer and write_buffer for the connection
    char read_buffer[1000],write_buffer[1000];
    int user_choice;
    ssize_t read_bytes,write_bytes;
    // printing welcome prompt 
    write_bytes = write(connection_descriptor,WELCOME_PROMPT,sizeof(WELCOME_PROMPT));
    if(write_bytes == -1)
        perror(WELCOME_PROMPT_ERROR);
    else{

        bzero(read_buffer,sizeof(read_buffer));
        read_bytes = read(connection_descriptor,read_buffer,sizeof(read_buffer));
        if(read_bytes == -1)
            perror(CLIENT_READ_ERROR);
        else if(read_bytes == 0)
            perror(EMPTY_INPUT_ERROR);
        else { 
            user_choice = atoi(read_buffer);
            switch(user_choice){
                case 1:
                    admin_operation_handler(connection_descriptor);
                    break;
                case 2:
                    user_operation_handler(connection_descriptor);
                    break;
                default:
                    write(connection_descriptor,INVALID_OPTION,sizeof(INVALID_OPTION));
                    break;
            }   
        }
    }
    printf(CLOSING_CONNECTION);

}