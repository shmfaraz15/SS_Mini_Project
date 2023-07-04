#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "../db/admin_credentials.h"
#include "./login_utility.h"
#include "./common_utility.h"
#include "../db/account_struct.h"
#include "../db/admin_credentials.h"

bool add_account(int connection_descriptor);
int add_user(int connection_descriptor,bool is_joint_account,int new_account_number);
bool delete_account(int connection_descriptor);
bool modify_user_info(int connection_descriptor);

bool admin_operation_handler(int connection_descriptor) {
    if(login_handler(true,connection_descriptor,NULL)){
        ssize_t write_bytes,read_bytes;
        char read_buffer[1000],write_buffer[1000];
        bzero(write_buffer,sizeof(write_buffer));
        strcpy(write_buffer,ADMIN_LOGIN_SUCCESS);
        while (1)
        {
            strcat(write_buffer,"\n");
            strcat(write_buffer,ADMIN_PROMPT);
            write_bytes = write(connection_descriptor,write_buffer,sizeof(write_buffer));
            if(write_bytes == -1){
                perror(CLIENT_WRITE_ERROR);
                return false;
            }
            bzero(write_buffer,sizeof(write_buffer));
            read_bytes = read(connection_descriptor,read_buffer,sizeof(read_buffer));
            if(read_bytes == -1)
            {
                perror(CLIENT_READ_ERROR);
                return false;
            }
            int option = atoi(read_buffer);
            switch (option)
            {
            case 1:
                add_account(connection_descriptor);
                break;
            case 2:
                 get_account_details(connection_descriptor, NULL);
                break;
            case 3:
                get_transaction_details(connection_descriptor,-1);
                break;
            case 4:
                get_user_details(connection_descriptor,-1);
                break;
            case 5:
                delete_account(connection_descriptor);
                break;
            case 6:
                modify_user_info(connection_descriptor);
                break;
            default:
                bzero(write_buffer,sizeof(write_buffer));
                strcat(write_buffer,"logging out $");
                write_bytes = write(connection_descriptor,write_buffer,strlen(write_buffer));
                return false;
            }
        }
        
    }
    else{
          return false;
    }
}
bool add_account(int connection_descriptor){
    size_t read_bytes , write_bytes;
    char read_buffer[1000],write_buffer[1000];
    struct Account new_account , prev_account;
    int account_file_descriptor = open("./db/ACCOUNT_FILE",O_RDONLY);
    if(account_file_descriptor == -1 && errno == ENOENT){
        new_account.account_number = 0;
    }
    else if (account_file_descriptor == -1){
        perror(FILE_OPEN_ERROR);
        return false;
    }
    else{
        int offset = lseek(account_file_descriptor, -sizeof(struct Account), SEEK_END);
        if (offset == -1)
        {
            perror("Error seeking to last Account record!");
            return false;
        }

        struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};
        int lock_status = fcntl(account_file_descriptor, F_SETLKW, &lock);
        if (lock_status == -1)
        {
            perror("Error obtaining read lock on Account record!");
            return false;
        }

        read_bytes = read(account_file_descriptor, &prev_account, sizeof(struct Account));
        if (read_bytes == -1)
        {
            perror("Error while reading Account record from file!");
            return false;
        }

        lock.l_type = F_UNLCK;
        fcntl(account_file_descriptor, F_SETLK, &lock);

        close(account_file_descriptor);

        new_account.account_number = prev_account.account_number + 1;
        printf("%d\n",new_account.account_number);
    }
    write_bytes = write(connection_descriptor,ADMIN_ACCOUNT_TYPE,strlen(ADMIN_ACCOUNT_TYPE));
    if(write_bytes == -1){
        perror(CLIENT_WRITE_ERROR);
        return false;
    }
    bzero(read_buffer,sizeof(read_buffer));
    read_bytes = read(connection_descriptor,&read_buffer,sizeof(read_buffer));
    if(read_bytes == -1){
        perror(CLIENT_READ_ERROR);
        return false;
    }
    int account_type = atoi(read_buffer);
    if(read_buffer[0]!='0' && read_buffer[0]!='1'){
        bzero(write_buffer,sizeof(write_buffer));
        strcpy(write_buffer,INVALID_OPTION);
        write(connection_descriptor,write_buffer,sizeof(write_buffer));
        return false;
    }
    new_account.account_type = atoi(read_buffer) == 1 ? 1 : 0;
    new_account.owners[0] = add_user(connection_descriptor,false,new_account.account_number);
    if(new_account.account_type)
        new_account.owners[1] = add_user(connection_descriptor,true,new_account.account_number);
    else
        new_account.owners[1] = -1;
    
    new_account.active_status = true;
    new_account.balance = 0;
    memset(new_account.transactions, -1,MAX_TRANSACTIONS * sizeof(int));

    account_file_descriptor = open("./db/ACCOUNT_FILE",O_CREAT | O_APPEND | O_WRONLY,S_IRWXU);
    if(account_file_descriptor == -1) {
        perror(FILE_OPEN_ERROR);
        return false;
    }
    write_bytes = write(account_file_descriptor, &new_account, sizeof(struct Account));
    if(write_bytes == -1){
        perror("Error while writing record in a file");
        return false;
    }
    close(account_file_descriptor);
    bzero(write_buffer,sizeof(write_buffer));
    sprintf(write_buffer,"%s%d","The newly created account's number is :",new_account.account_number);
    strcat(write_buffer, "\nReturning to the main menu ...^");
    write_bytes = write(connection_descriptor,write_buffer,sizeof(write_buffer));
    read_bytes = read(connection_descriptor,read_buffer,sizeof(read_buffer));
    return true;

}
int add_user(int connection_descriptor,bool is_joint_account,int new_account_number) {
    size_t read_bytes , write_bytes;
    char read_buffer[1000], write_buffer[1000];
    struct User new_user, previous_user;
    int user_file_desciptor = open("./db/USER_FILE",O_RDONLY);
    if(user_file_desciptor == -1 && errno == ENOENT) {
        new_user.user_id = 0;
    }
    else if(user_file_desciptor == -1) {
        perror(FILE_OPEN_ERROR);
        return false;
    }
    else{
        int offset = lseek(user_file_desciptor, -sizeof(struct User), SEEK_END);
        if (offset == -1)
        {
            perror("Error seeking to last user record!");
            return false;
        }

        struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct User), getpid()};
        int lock_status = fcntl(user_file_desciptor, F_SETLKW, &lock);
        if (lock_status == -1)
        {
            perror("Error obtaining read lock on user record!");
            return false;
        }

        read_bytes = read(user_file_desciptor, &previous_user, sizeof(struct User));
        if (read_bytes == -1)
        {
            perror("Error while reading user record from file!");
            return false;
        }

        lock.l_type = F_UNLCK;
        fcntl(user_file_desciptor, F_SETLK, &lock);

        close(user_file_desciptor);

        new_user.user_id= previous_user.user_id+ 1;
    }
    bzero(write_buffer,sizeof(write_buffer));
    if(is_joint_account)
        strcat(write_buffer,"Enter details of second user \n Enter name of the user :");
    else
        strcat(write_buffer,"Enter details of  user \n Enter name of the user :");
    write_bytes = write(connection_descriptor,write_buffer,sizeof(write_buffer));
    if(write_bytes == -1)
    {
        perror(CLIENT_WRITE_ERROR);
        return false;
    }
    bzero(read_buffer,sizeof(read_buffer));
    read_bytes = read(connection_descriptor,read_buffer,sizeof(read_buffer));
    if(read_bytes == -1){
        perror(CLIENT_READ_ERROR);
        return false;
    }
    strcpy(new_user.user_name,read_buffer);

    write_bytes = write(connection_descriptor,USER_GENDER_QUESTION,sizeof(USER_GENDER_QUESTION));
    if(write_bytes == -1){
        perror(CLIENT_WRITE_ERROR);
        return false;
    }
    bzero(read_buffer,sizeof(read_buffer));
    read_bytes = read(connection_descriptor,read_buffer,sizeof(read_buffer));
    if(read_bytes == -1){
        perror(CLIENT_WRITE_ERROR);
        return false;
    }
    if(read_buffer[0] == 'M' || read_buffer[0]=='F' || read_buffer[0] == 'O')
        new_user.gender = read_buffer[0];
    else {
        write_bytes = write(connection_descriptor,INVALID_GENDER,sizeof(INVALID_GENDER));
        read_bytes = read(connection_descriptor,read_buffer,sizeof(read_buffer));
        return false;
    }
    bzero(write_buffer,sizeof(write_buffer));
    write_bytes = write(connection_descriptor,USER_AGE_QUESTION,sizeof(USER_AGE_QUESTION));
    if(write_bytes == -1)
    {
        perror(CLIENT_WRITE_ERROR);
        return false;
    }
    bzero(read_buffer,sizeof(read_buffer));
    read_bytes = read(connection_descriptor,read_buffer,sizeof(read_buffer));
    if(read_bytes == -1){
        perror(CLIENT_READ_ERROR);
         return false;
    }
    int user_age = atoi(read_buffer);
    if (user_age == 0)
    {
        bzero(write_buffer, sizeof(write_buffer));
        
        write_bytes = write(connection_descriptor, INVALID_AGE, strlen(INVALID_AGE));
        if (write_bytes == -1)
        {
            perror(CLIENT_WRITE_ERROR);
            return false;
        }
        read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer)); 
        return false;
    
    }
    new_user.age = user_age;
    char password[20];
    if(!is_joint_account){
    bzero(write_buffer,sizeof(write_buffer));
    write_bytes = write(connection_descriptor, NEW_PASSWORD_REQUEST, strlen(NEW_PASSWORD_REQUEST));
    if (write_bytes == -1)
    {
        perror(CLIENT_WRITE_ERROR);
        return false;
    }
    bzero(read_buffer, sizeof(read_buffer));
    read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer));
    if (read_bytes == -1)
    {
        perror(CLIENT_READ_ERROR);
        return false;
    }

    strcpy(password, read_buffer);

    write_bytes = write(connection_descriptor, NEW_PASSWORD_RE_ENTER, strlen(NEW_PASSWORD_RE_ENTER));
    if (write_bytes == -1)
    {
        perror(CLIENT_WRITE_ERROR);
        return false;
    }
    bzero(read_buffer, sizeof(read_buffer));
    read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer));
    if (read_bytes == -1)
    {
        perror(CLIENT_READ_ERROR);
        return false;
    }
    if (strcmp(read_buffer,password)!= 0)
    {
        perror(PASSWORD_DO_NOT_MATCH);
        return false;
    }
    }
    else{

        strcpy(password,"hellow");
    }
    new_user.account_number = new_account_number;
    strcpy(new_user.login_id,new_user.user_name);
    strcat(new_user.login_id,"-");
    sprintf(write_buffer,"%d",new_user.user_id);
    strcat(new_user.login_id,write_buffer);
    strcpy(new_user.password,password);
    new_user.account_opening_date = time(NULL);;
    user_file_desciptor = open("./db/USER_FILE",O_CREAT | O_APPEND | O_WRONLY,S_IRWXU);
    if(user_file_desciptor == -1){
        perror("here error ");
        return false;
    }
    write_bytes = write(user_file_desciptor,&new_user,sizeof(new_user));
    if(write_bytes == -1){
        perror("Error while writing in file");
        return false;
    }
    close(user_file_desciptor);
    bzero(write_buffer,sizeof(write_buffer));
    sprintf(write_buffer,"%s%s-%d\n%s%s","The new user id is: ",new_user.user_name,new_user.user_id,"The password for new user is please save for further login :- ",password);
    strcat(write_buffer,"^");
    write_bytes = write(connection_descriptor,write_buffer,strlen(write_buffer));
    if(write_bytes == -1) {
        perror(CLIENT_WRITE_ERROR);
        return false;
    }
    read_bytes = read(connection_descriptor,read_buffer,strlen(read_buffer));
    return new_user.user_id;

}

bool delete_account(int connection_descriptor)
{
    ssize_t read_bytes, write_bytes;
    char read_buffer[1000], write_buffer[1000];

    struct Account account;

    write_bytes = write(connection_descriptor, DELETE_ACCOUNT_NUMBER_QUESTION, strlen(DELETE_ACCOUNT_NUMBER_QUESTION));
    if (write_bytes == -1)
    {
        perror("Error writing ADMIN_DEL_ACCOUNT_NO to client!");
        return false;
    }

    bzero(read_buffer, sizeof(read_buffer));
    read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer));
    if (read_bytes == -1)
    {
        perror("Error reading account number response from the client!");
        return false;
    }

    int accountNumber = atoi(read_buffer);

    int account_file_descriptor = open(ACCOUNT_FILE, O_RDONLY);
    if (account_file_descriptor == -1)
    {
        // Account record doesn't exist
        bzero(write_buffer, sizeof(write_buffer));
        strcpy(write_buffer, INVALID_ACCOUNT_ID);
        strcat(write_buffer, "^");
        write_bytes = write(connection_descriptor, write_buffer, strlen(write_buffer));
        if (write_bytes == -1)
        {
            perror("Error while writing ACCOUNT_ID_DOESNT_EXIT message to client!");
            return false;
        }
        read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer)); 
        return false;
    }


    int offset = lseek(account_file_descriptor, accountNumber * sizeof(struct Account), SEEK_SET);
    if (errno == EINVAL)
    {
        // user record doesn't exist
        bzero(write_buffer, sizeof(write_buffer));
        strcpy(write_buffer, INVALID_ACCOUNT_ID);
        strcat(write_buffer, "^");
        write_bytes = write(connection_descriptor, write_buffer, strlen(write_buffer));
        if (write_bytes == -1)
        {
            perror("Error while writing ACCOUNT_ID_DOESNT_EXIT message to client!");
            return false;
        }
        read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer)); 
        return false;
    }
    else if (offset == -1)
    {
        perror("Error while seeking to required account record!");
        return false;
    }

    struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};
    int lock_status = fcntl(account_file_descriptor, F_SETLKW, &lock);
    if (lock_status == -1)
    {
        perror("Error obtaining read lock on Account record!");
        return false;
    }

    read_bytes = read(account_file_descriptor, &account, sizeof(struct Account));
    if (read_bytes == -1)
    {
        perror("Error while reading Account record from file!");
        return false;
    }

    lock.l_type = F_UNLCK;
    fcntl(account_file_descriptor, F_SETLK, &lock);

    close(account_file_descriptor);

    bzero(write_buffer, sizeof(write_buffer));
    if (account.balance == 0)
    {
        // No money, hence can close account
        account.active_status = false;
        account_file_descriptor = open(ACCOUNT_FILE, O_WRONLY);
        if (account_file_descriptor == -1)
        {
            perror("Error opening Account file in write mode!");
            return false;
        }

        offset = lseek(account_file_descriptor, accountNumber * sizeof(struct Account), SEEK_SET);
        if (offset == -1)
        {
            perror("Error seeking to the Account!");
            return false;
        }

        lock.l_type = F_WRLCK;
        lock.l_start = offset;

        int lock_status = fcntl(account_file_descriptor, F_SETLKW, &lock);
        if (lock_status == -1)
        {
            perror("Error obtaining write lock on the Account file!");
            return false;
        }

        write_bytes = write(account_file_descriptor, &account, sizeof(struct Account));
        if (write_bytes == -1)
        {
            perror("Error deleting account record!");
            return false;
        }

        lock.l_type = F_UNLCK;
        fcntl(account_file_descriptor, F_SETLK, &lock);

        strcpy(write_buffer, ACCOUNT_DELETE_SUCCESS);
    }
    else
        // Account has some money ask user to withdraw it
        strcpy(write_buffer, ACCOUNT_DELETE_SUCCESS);
    write_bytes = write(connection_descriptor, write_buffer, strlen(write_buffer));
    if (write_bytes == -1)
    {
        perror("Error while writing final DEL message to client!");
        return false;
    }
    read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer)); 

    return true;
}

bool modify_user_info(int connection_descriptor)
{
    ssize_t read_bytes, write_bytes;
    char read_buffer[1000], write_buffer[1000];

    struct User user;

    int user_id;

    off_t offset;
    int lock_status;

    write_bytes = write(connection_descriptor, MODIFY_USER_ID_QUESTION, strlen(MODIFY_USER_ID_QUESTION));
    if (write_bytes == -1)
    {
        perror(CLIENT_WRITE_ERROR);
        return false;
    }
    bzero(read_buffer, sizeof(read_buffer));
    read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer));
    if (read_bytes == -1)
    {
        perror(CLIENT_READ_ERROR);
        return false;
    }

    user_id = atoi(read_buffer);

    int user_file_descriptor = open(USER_FILE, O_RDONLY);
    if (user_file_descriptor == -1)
    {
        // user File doesn't exist
        bzero(write_buffer, sizeof(write_buffer));
        strcpy(write_buffer, INVALID_USER_ID);
        strcat(write_buffer, "^");
        write_bytes = write(connection_descriptor, write_buffer, strlen(write_buffer));
        if (write_bytes == -1)
        {
            perror(CLIENT_WRITE_ERROR);
            return false;
        }
        read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer)); 
        return false;
    }
    
    offset = lseek(user_file_descriptor, user_id * sizeof(struct User), SEEK_SET);
    if (errno == EINVAL)
    {
        // user record doesn't exist
        bzero(write_buffer, sizeof(write_buffer));
        strcpy(write_buffer, INVALID_USER_ID);
        strcat(write_buffer, "^");
        write_bytes = write(connection_descriptor, write_buffer, strlen(write_buffer));
        if (write_bytes == -1)
        {
            perror(CLIENT_WRITE_ERROR);
            return false;
        }
        read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer)); 
        return false;
    }
    else if (offset == -1)
    {
        perror(CLIENT_READ_ERROR);
        return false;
    }

    struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct User), getpid()};

    // Lock the record to be read
    lock_status = fcntl(user_file_descriptor, F_SETLKW, &lock);
    if (lock_status == -1)
    {
        perror("Couldn't obtain lock on user record!");
        return false;
    }

    read_bytes = read(user_file_descriptor, &user, sizeof(struct User));
    if (read_bytes == -1)
    {
        perror("Error while reading user record from the file!");
        return false;
    }

    // Unlock the record
    lock.l_type = F_UNLCK;
    fcntl(user_file_descriptor, F_SETLK, &lock);

    close(user_file_descriptor);

    write_bytes = write(connection_descriptor, MODIFY_INFORMATION_OPTION, strlen(MODIFY_INFORMATION_OPTION));
    if (write_bytes == -1)
    {
        perror(CLIENT_WRITE_ERROR);
        return false;
    }
    read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer));
    if (read_bytes == -1)
    {
        perror(CLIENT_READ_ERROR);
        return false;
    }

    int choice = atoi(read_buffer);
    if (choice == 0)
    { // A non-numeric string was passed to atoi
        bzero(write_buffer, sizeof(write_buffer));
        strcpy(write_buffer, INVALID_OPTION);
        write_bytes = write(connection_descriptor, write_buffer, strlen(write_buffer));
        if (write_bytes == -1)
        {
            perror(CLIENT_WRITE_ERROR);
            return false;
        }
        read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer)); 
        return false;
    }

    bzero(read_buffer, sizeof(read_buffer));
    switch (choice)
    {
    case 1:
        write_bytes = write(connection_descriptor, MODIFY_NAME_MESSAGE, strlen(MODIFY_NAME_MESSAGE));
        if (write_bytes == -1)
        {
            perror(CLIENT_WRITE_ERROR);
            return false;
        }
        read_bytes = read(connection_descriptor, &read_buffer, sizeof(read_buffer));
        if (read_bytes == -1)
        {
            perror(CLIENT_READ_ERROR);
            return false;
        }
        strcpy(user.user_name, read_buffer);
        break;
    case 2:
        write_bytes = write(connection_descriptor, MODIFY_AGE_MESSAGE, strlen(MODIFY_AGE_MESSAGE));
        if (write_bytes == -1)
        {
            perror(CLIENT_WRITE_ERROR);
            return false;
        }
        read_bytes = read(connection_descriptor, &read_buffer, sizeof(read_buffer));
        if (read_bytes == -1)
        {
            perror(CLIENT_READ_ERROR);
            return false;
        }
        int updatedAge = atoi(read_buffer);
        if (updatedAge == 0)
        {
            // Either client has sent age as 0 (which is invalid) or has entered a non-numeric string
            bzero(write_buffer, sizeof(write_buffer));
            strcpy(write_buffer, INVALID_OPTION);
            write_bytes = write(connection_descriptor, write_buffer, strlen(write_buffer));
            if (write_bytes == -1)
            {
                perror(CLIENT_WRITE_ERROR);
                return false;
            }
            read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer)); 
            return false;
        }
        user.age = updatedAge;
        break;
    case 3:
        write_bytes = write(connection_descriptor, MODIFY_GENDER_MESSAGE, strlen(MODIFY_GENDER_MESSAGE));
        if (write_bytes == -1)
        {
            perror(CLIENT_WRITE_ERROR);
            return false;
        }
        read_bytes = read(connection_descriptor, &read_buffer, sizeof(read_buffer));
        if (read_bytes == -1)
        {
            perror(CLIENT_READ_ERROR);
            return false;
        }
        user.gender = read_buffer[0];
        break;
    default:
        bzero(write_buffer, sizeof(write_buffer));
        strcpy(write_buffer, INVALID_OPTION);
        write_bytes = write(connection_descriptor, write_buffer, strlen(write_buffer));
        if (write_bytes == -1)
        {
            perror(CLIENT_WRITE_ERROR);
            return false;
        }
        read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer)); 
        return false;
    }

    user_file_descriptor = open(USER_FILE, O_WRONLY);
    if (user_file_descriptor == -1)
    {
        perror(FILE_OPEN_ERROR);
        return false;
    }
    offset = lseek(user_file_descriptor, user_id * sizeof(struct User), SEEK_SET);
    if (offset == -1)
    {
        perror("Error while seeking to required user record!");
        return false;
    }

    lock.l_type = F_WRLCK;
    lock.l_start = offset;
    lock_status = fcntl(user_file_descriptor, F_SETLKW, &lock);
    if (lock_status == -1)
    {
        perror("Error while obtaining write lock on user record!");
        return false;
    }

    write_bytes = write(user_file_descriptor, &user, sizeof(struct User));
    if (write_bytes == -1)
    {
        perror("Error while writing update user info into file");
    }

    lock.l_type = F_UNLCK;
    fcntl(user_file_descriptor, F_SETLKW, &lock);

    close(user_file_descriptor);

    write_bytes = write(connection_descriptor, MODIFY_SUCCESS_MESSAGE, strlen(MODIFY_SUCCESS_MESSAGE));
    if (write_bytes == -1)
    {
        perror(CLIENT_WRITE_ERROR);
        return false;
    }
    read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer)); 

    return true;
}