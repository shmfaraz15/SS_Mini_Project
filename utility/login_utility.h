#ifndef LOGIN_FUNCTION
#define LOGIN_FUNCTION
#include<stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>  
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../db/admin_credentials.h"
#include "../db/user_struct.h"

bool login_handler(bool is_admin, int connection_descriptor, struct User *user_id_ptr) {
    ssize_t read_bytes, write_bytes;
    char read_buffer[1000], write_buffer[1000];
    char temp_buffer[1000];
    struct  User user;
    int user_id;
    bzero(read_buffer, sizeof(read_buffer));
    bzero(write_buffer,sizeof(write_buffer));
    strcpy(write_buffer, LOGIN_PROMPT);
    write_bytes = write(connection_descriptor,write_buffer,strlen(write_buffer));
    if(write_bytes == -1) {
        perror(CLIENT_WRITE_ERROR);
        return false;
    }
    read_bytes = read(connection_descriptor,read_buffer,sizeof(read_buffer));
    if(read_bytes == -1){
        perror(CLIENT_READ_ERROR);
        return false;
    }
    bool user_found = false;
    if(is_admin){
        if(strcmp(read_buffer,ADMIN_LOGIN_ID)==0){
            user_found = true;
        }
    }
    else {
        bzero(temp_buffer,sizeof(temp_buffer));
        strcpy(temp_buffer,read_buffer);
        strtok(temp_buffer,"-");
        char* token = strtok(NULL,"-");
        if(token == NULL) {
             write_bytes = write(connection_descriptor, INVALID_USER_ID, strlen(INVALID_USER_ID));
             return false;
        }
        user_id = atoi(token);
        int user_file_descriptor = open(USER_FILE, O_RDONLY);
        if (user_file_descriptor == -1)
        {
            perror("Error opening customer file in read mode!");
            return false;
        }

        off_t offset = lseek(user_file_descriptor, user_id * sizeof(struct User), SEEK_SET);
        if (offset >= 0)
        {
            struct flock lock = {F_RDLCK, SEEK_SET, user_id * sizeof(struct User), sizeof(struct User), getpid()};

            int lock_status = fcntl(user_file_descriptor, F_SETLKW, &lock);
            if (lock_status == -1)
            {
                perror("Error obtaining read lock on user record!");
                return false;
            }

            read_bytes = read(user_file_descriptor, &user, sizeof(struct User));
            if (read_bytes == -1)
            {
                perror("Error reading user record from file!");
            }

            lock.l_type = F_UNLCK;
            fcntl(user_file_descriptor, F_SETLK, &lock);

            if (strcmp(user.login_id, read_buffer) == 0)
                user_found = true;
            close(user_file_descriptor);
        }
        else
        {
            write_bytes = write(connection_descriptor, INVALID_USER_ID, strlen(INVALID_USER_ID));
            return false;
        }
    }
    if(user_found) {
        bzero(write_buffer,sizeof(write_buffer));
        write_bytes = write(connection_descriptor,PASSWORD_REQUEST,strlen(PASSWORD_REQUEST));
        if(write_bytes == -1)
        {
            perror(CLIENT_WRITE_ERROR);
            return false;
        }
        bzero(read_buffer,sizeof(read_buffer));
        read_bytes = read(connection_descriptor,read_buffer,sizeof(read_buffer));
        if(read_bytes == 1) {
            perror(CLIENT_READ_ERROR);
            return false;
        }
        if(is_admin){
            if(strcmp(read_buffer,ADMIN_PASSWORD)==0)
                return true;
            else{
                bzero(write_buffer,sizeof(write_buffer));
                write_bytes = write(connection_descriptor, INVALID_PASSWORD, strlen(INVALID_PASSWORD));
                return false;
            }
        }
        else{
            if(strcmp(read_buffer,user.password) == 0){
                *user_id_ptr = user;
                return true;
            }
            else{
                bzero(write_buffer,sizeof(write_buffer));
                write_bytes = write(connection_descriptor, INVALID_PASSWORD, strlen(INVALID_PASSWORD));
                return false;
            }
        }
    }
    else {
        printf("here\n");
        bzero(write_buffer,sizeof(write_buffer));
        write_bytes = write(connection_descriptor, INVALID_USER_ID, strlen(INVALID_USER_ID));
        return false;
    }
}
#endif