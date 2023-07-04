#ifndef COMMON_FUNCTIONS
#define COMMON_FUNCTIONS
#include <sys/types.h>  
#include <sys/stat.h> 
#include <fcntl.h>    
#include <errno.h>     

#include "../db/account_struct.h"
#include "../db/user_struct.h"
#include "../db/transaction_struct.h"
#include "../messages/error_message.h"
#include "../messages/general_message.h"
#include "../db/admin_credentials.h"

bool get_account_details(int connection_descriptor, struct Account *user_account);
bool get_user_details(int connection_descriptor, int user_id);
bool get_transaction_details(int connection_descriptor, int account_number);

bool get_account_details(int connection_descriptor, struct Account *user_account) {
    ssize_t read_bytes, write_bytes;            
    char read_buffer[1000], write_buffer[1000]; 
    char temp_buffer[1000];

    int account_number, account_file_descriptor;
    struct Account account;

    if (user_account == NULL)
    {

        write_bytes = write(connection_descriptor, ACCOUNT_NUMBER_QUESTION, strlen(ACCOUNT_NUMBER_QUESTION));
        if (write_bytes == -1)
        {
            perror(CLIENT_WRITE_ERROR);
            return false;
        }

        bzero(read_buffer, sizeof(read_buffer));
        read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer));
        if (read_bytes == -1)
        {
            perror("Error reading account number response from client!");
            return false;
        }

        account_number = atoi(read_buffer);
    }
    else
        account_number = user_account->account_number;

    account_file_descriptor = open("./db/ACCOUNT_FILE", O_RDONLY);
    if (account_file_descriptor == -1)
    {
        // Account record doesn't exist
        bzero(write_buffer, sizeof(write_buffer));
        strcpy(write_buffer, INVALID_ACCOUNT_ID);
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

    int offset = lseek(account_file_descriptor, account_number * sizeof(struct Account), SEEK_SET);
    if (offset == -1 && errno == EINVAL)
    {
        // Account record doesn't exist
        bzero(write_buffer, sizeof(write_buffer));
        strcpy(write_buffer, INVALID_ACCOUNT_ID);
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
        perror("Error while seeking to required account record!");
        return false;
    }

    struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};

    int lock_status = fcntl(account_file_descriptor, F_SETLKW, &lock);
    if (lock_status == -1)
    {
        perror("Error obtaining read lock on account record!");
        return false;
    }

    read_bytes = read(account_file_descriptor, &account, sizeof(struct Account));
    if (read_bytes == -1)
    {
        perror("Error reading account record from file!");
        return false;
    }

    lock.l_type = F_UNLCK;
    fcntl(account_file_descriptor, F_SETLK, &lock);

    if (user_account != NULL)
    {
        *user_account = account;
        return true;
    }

    bzero(write_buffer, sizeof(write_buffer));
    sprintf(write_buffer, "Account Details - \n\tAccount Number : %d\n\tAccount Type : %s\n\tAccount Status : %s", account.account_number, (account.account_type ? "Joint" : "Regular"), (account.active_status) ? "Active" : "Deactived");
    if (account.active_status)
    {
        sprintf(temp_buffer, "\n\tAccount Balance:₹ %ld", account.balance);
        strcat(write_buffer, temp_buffer);
    }

    sprintf(temp_buffer, "\n\tPrimary Owner ID: %d", account.owners[0]);
    strcat(write_buffer, temp_buffer);
    if (account.owners[1] != -1)
    {
        sprintf(temp_buffer, "\n\tSecondary Owner ID: %d", account.owners[1]);
        strcat(write_buffer, temp_buffer);
    }

    strcat(write_buffer, "\n^");

    write_bytes = write(connection_descriptor, write_buffer, strlen(write_buffer));
    read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer)); 

    return true;
}

bool get_user_details(int connection_descriptor, int user_id)
{
    ssize_t read_bytes, write_bytes;             
    char read_buffer[1000], write_buffer[10000]; 
    char temp_buffer[1000];

    struct User user;
    int user_file_descriptor;
    struct flock lock = {F_RDLCK, SEEK_SET, 0, sizeof(struct Account), getpid()};

    if (user_id == -1)
    {
        write_bytes = write(connection_descriptor, USER_ID_QUESTION, strlen(USER_ID_QUESTION));
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
    }

    user_file_descriptor = open(USER_FILE, O_RDONLY);
    if (user_file_descriptor == -1)
    {
        // user File doesn't exist
        bzero(write_buffer, sizeof(write_buffer));
        strcpy(write_buffer, INVALID_USER_ID);
        strcat(write_buffer, "^");
        write_bytes = write(connection_descriptor, write_buffer, strlen(write_buffer));
        if (write_bytes == -1)
        {
            perror("Error while writing user_ID_DOESNT_EXIT message to client!");
            return false;
        }
        read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer)); 
        return false;
    }
    int offset = lseek(user_file_descriptor, user_id * sizeof(struct User), SEEK_SET);
    if (errno == EINVAL)
    {
        // user record doesn't exist
        bzero(write_buffer, sizeof(write_buffer));
        strcpy(write_buffer, INVALID_USER_ID);
        strcat(write_buffer, "^");
        write_bytes = write(connection_descriptor, write_buffer, strlen(write_buffer));
        if (write_bytes == -1)
        {
            perror("Error while writing user_ID_DOESNT_EXIT message to client!");
            return false;
        }
        read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer)); 
        return false;
    }
    else if (offset == -1)
    {
        perror("Error while seeking to required user record!");
        return false;
    }
    lock.l_start = offset;

    int lockingStatus = fcntl(user_file_descriptor, F_SETLKW, &lock);
    if (lockingStatus == -1)
    {
        perror("Error while obtaining read lock on the user file!");
        return false;
    }

    read_bytes = read(user_file_descriptor, &user, sizeof(struct User));
    if (read_bytes == -1)
    {
        perror("Error reading user record from file!");
        return false;
    }

    lock.l_type = F_UNLCK;
    fcntl(user_file_descriptor, F_SETLK, &lock);

    bzero(write_buffer, sizeof(write_buffer));
    struct tm account_opening_time = *localtime(&(user.account_opening_date));
    sprintf(write_buffer, "user Details - \n\tUser ID : %d\n\tName : %s\n\tGender : %c\n\tAge: %d\n\tAccount Number : %d\n\tLogin ID : %s\n\tAccount Opening Date:-  %d:%d %d/%d/%d ", user.user_id, user.user_name, user.gender, user.age, user.account_number, user.login_id, account_opening_time.tm_hour, account_opening_time.tm_min, account_opening_time.tm_mday, (account_opening_time.tm_mon + 1), (account_opening_time.tm_year + 1900));

    strcat(write_buffer, "\n\nYou'll now be redirected to the main menu...^");

    write_bytes = write(connection_descriptor, write_buffer, strlen(write_buffer));
    if (write_bytes == -1)
    {
        perror("Error writing user info to client!");
        return false;
    }

    read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer)); 
    return true;
}

bool get_transaction_details(int connection_descriptor, int account_number)
{

    ssize_t read_bytes, write_bytes;                               
    char read_buffer[1000], write_buffer[10000], temp_buffer[1000]; 

    struct Account account;

    if (account_number == -1)
    {
        // Get the account_number
        write_bytes = write(connection_descriptor, ACCOUNT_NUMBER_QUESTION, strlen(ACCOUNT_NUMBER_QUESTION));
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

        account.account_number = atoi(read_buffer);
    }
    else
        account.account_number = account_number;

    if (get_account_details(connection_descriptor, &account))
    {
        int iter;

        struct Transaction transaction;
        struct tm transaction_time;

        bzero(write_buffer, sizeof(read_buffer));

        int transaction_file_descriptor = open("./db/TRANSACTION_FILE", O_RDONLY);
        if (transaction_file_descriptor == -1)
        {
            perror("Error while opening transaction file!");
            write(connection_descriptor, TRANSACTIONS_NOT_FOUND, strlen(TRANSACTIONS_NOT_FOUND));
            read(connection_descriptor, read_buffer, sizeof(read_buffer)); 
            return false;
        }

        for (iter = 0; iter < MAX_TRANSACTIONS && account.transactions[iter] != -1; iter++)
        {

            int offset = lseek(transaction_file_descriptor, account.transactions[iter] * sizeof(struct Transaction), SEEK_SET);
            if (offset == -1)
            {
                perror("Error while seeking to required transaction record!");
                return false;
            }

            struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Transaction), getpid()};

            int lockingStatus = fcntl(transaction_file_descriptor, F_SETLKW, &lock);
            if (lockingStatus == -1)
            {
                perror("Error obtaining read lock on transaction record!");
                return false;
            }

            read_bytes = read(transaction_file_descriptor, &transaction, sizeof(struct Transaction));
            if (read_bytes == -1)
            {
                perror("Error reading transaction record from file!");
                return false;
            }

            lock.l_type = F_UNLCK;
            fcntl(transaction_file_descriptor, F_SETLK, &lock);

            transaction_time = *localtime(&(transaction.transaction_time));

            bzero(temp_buffer, sizeof(temp_buffer));
            sprintf(temp_buffer, "Details of transaction %d - \n\t Date : %d:%d %d/%d/%d \n\t Operation : %s \n\t Balance - \n\t\t Before : %ld \n\t\t After : %ld \n\t\t Difference : %ld\n", (iter + 1), transaction_time.tm_hour, transaction_time.tm_min, transaction_time.tm_mday, (transaction_time.tm_mon + 1), (transaction_time.tm_year + 1900), (transaction.operation ? "Deposit" : "Withdraw"), transaction.old_balance, transaction.new_balance, (transaction.new_balance - transaction.old_balance));

            if (strlen(write_buffer) == 0)
                strcpy(write_buffer, temp_buffer);
            else
                strcat(write_buffer, temp_buffer);
        }

        close(transaction_file_descriptor);

        if (strlen(write_buffer) == 0)
        {
            write(connection_descriptor, TRANSACTIONS_NOT_FOUND, strlen(TRANSACTIONS_NOT_FOUND));
            read(connection_descriptor, read_buffer, sizeof(read_buffer)); 
            return false;
        }
        else
        {
            strcat(write_buffer, "^");
            write_bytes = write(connection_descriptor, write_buffer, strlen(write_buffer));
            read(connection_descriptor, read_buffer, sizeof(read_buffer)); 
        }
    }
}
#endif