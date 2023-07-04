#include <sys/types.h>
#include <sys/stat.h>
#include "../db/admin_credentials.h"
#include "./login_utility.h"
#include "./common_utility.h"
#include <sys/ipc.h>
#include <sys/sem.h>

bool user_operation_handler(int connection_descriptor);
bool deposit(int connection_descriptor);
bool withdraw(int connection_descriptor);
bool get_balance(int connection_descriptor);
bool change_password(int connection_descriptor);
bool lock_critical_section(struct sembuf *sem_op);
bool unlock_critical_section(struct sembuf *sem_op);
void write_transaction_to_array(int *transaction_list, int ID);
int write_transaction_to_file(int account_number, long int old_balance, long int new_balance, bool operation);
struct User logged_in_user;
int sem_identifier;

bool user_operation_handler(int connection_descriptor)
{
    if (login_handler(false, connection_descriptor,&logged_in_user))
    {
        ssize_t write_bytes, read_bytes;            
        char read_buffer[1000], write_buffer[1000]; 

        key_t semKey = ftok("./db/user.h", logged_in_user.account_number);

        union semun
        {
            int val; 
        } semSet;

        int semctlStatus;
        sem_identifier = semget(semKey, 1, 0); 
        if (sem_identifier == -1)
        {
            sem_identifier = semget(semKey, 1, IPC_CREAT | 0700); 
            if (sem_identifier == -1)
            {
                perror("Error while creating semaphore!");
                _exit(1);
            }

            semSet.val = 1; //binary
            semctlStatus = semctl(sem_identifier, 0, SETVAL, semSet);
            if (semctlStatus == -1)
            {
                perror("Error while initializing a binary sempahore!");
                _exit(1);
            }
        }

        bzero(write_buffer, sizeof(write_buffer));
        strcpy(write_buffer, CUSTOMER_LOGIN_SUCCESS);
        while (1)
        {
            strcat(write_buffer, "\n");
            strcat(write_buffer, USER_MENU);
            write_bytes = write(connection_descriptor, write_buffer, strlen(write_buffer));
            if (write_bytes == -1)
            {
                perror(CLIENT_WRITE_ERROR);
                return false;
            }
            bzero(write_buffer, sizeof(write_buffer));

            bzero(read_buffer, sizeof(read_buffer));
            read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer));
            if (read_bytes == -1)
            {
                perror("Error while reading client's choice for CUSTOMER_MENU");
                return false;
            }
            int choice = atoi(read_buffer);
            switch (choice)
            {
            case 1:
                get_user_details(connection_descriptor, logged_in_user.user_id);
                break;
            case 2:
                deposit(connection_descriptor);
                break;
            case 3:
                withdraw(connection_descriptor);
                break;
            case 4:
                get_balance(connection_descriptor);
                break;
            case 5:
                get_transaction_details(connection_descriptor, logged_in_user.account_number);
                break;
            case 6:
                change_password(connection_descriptor);
                break;
            default:
                write_bytes = write(connection_descriptor, USER_LOGOUT, strlen(USER_LOGOUT));
                return false;
            }
        }
    }
    else
    {
        return false;
    }
    return true;
}

bool deposit(int connection_descriptor)
{
    char read_buffer[1000], write_buffer[1000];
    ssize_t read_bytes, write_bytes;

    struct Account account;
    account.account_number = logged_in_user.account_number;

    long int depositAmount = 0;

    // Lock the critical section
    struct sembuf sem_op;
    lock_critical_section(&sem_op);

    if (get_account_details(connection_descriptor, &account))
    {
        
        if (account.active_status)
        {

            write_bytes = write(connection_descriptor, DEPOSIT_AMOUNT_QUESTION, strlen(DEPOSIT_AMOUNT_QUESTION));
            if (write_bytes == -1)
            {
                perror("Error writing DEPOSIT_AMOUNT to client!");
                unlock_critical_section(&sem_op);
                return false;
            }

            bzero(read_buffer, sizeof(read_buffer));
            read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer));
            if (read_bytes == -1)
            {
                perror("Error reading deposit money from client!");
                unlock_critical_section(&sem_op);
                return false;
            }

            depositAmount = atol(read_buffer);
            if (depositAmount != 0)
            {

                int new_transaction_id = write_transaction_to_file(account.account_number, account.balance, account.balance + depositAmount, 1);
                write_transaction_to_array(account.transactions, new_transaction_id);

                account.balance += depositAmount;

                int account_file_descriptor = open("./db/ACCOUNT_FILE", O_WRONLY);
                off_t offset = lseek(account_file_descriptor, account.account_number * sizeof(struct Account), SEEK_SET);

                struct flock lock = {F_WRLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};
                int lockingStatus = fcntl(account_file_descriptor, F_SETLKW, &lock);
                if (lockingStatus == -1)
                {
                    perror("Error obtaining write lock on account file!");
                    unlock_critical_section(&sem_op);
                    return false;
                }

                write_bytes = write(account_file_descriptor, &account, sizeof(struct Account));
                if (write_bytes == -1)
                {
                    perror("Error storing updated deposit money in account record!");
                    unlock_critical_section(&sem_op);
                    return false;
                }

                lock.l_type = F_UNLCK;
                fcntl(account_file_descriptor, F_SETLK, &lock);

                write(connection_descriptor, DEPOSIT_AMOUNT_SUCCESS, strlen(DEPOSIT_AMOUNT_SUCCESS));
                read(connection_descriptor, read_buffer, sizeof(read_buffer)); // Dummy read

                get_balance(connection_descriptor);

                unlock_critical_section(&sem_op);

                return true;
            }
            else
                write_bytes = write(connection_descriptor, INVALID_DEPOSIT_AMOUNT, strlen(INVALID_DEPOSIT_AMOUNT));
        }
        else
            write(connection_descriptor, ACCOUNT_DEACTIVATED_MESSAGE, strlen(ACCOUNT_DEACTIVATED_MESSAGE));
        read(connection_descriptor, read_buffer, sizeof(read_buffer)); // Dummy read

        unlock_critical_section(&sem_op);
    }
    else
    {
        unlock_critical_section(&sem_op);
        return false;
    }
}

bool withdraw(int connection_descriptor)
{
    char read_buffer[1000], write_buffer[1000];
    ssize_t read_bytes, write_bytes;

    struct Account account;
    account.account_number = logged_in_user.account_number;

    long int withdraw_amount = 0;

    // Lock the critical section
    struct sembuf sem_op;
    lock_critical_section(&sem_op);

    if (get_account_details(connection_descriptor, &account))
    {
        if (account.active_status)
        {

            write_bytes = write(connection_descriptor, WITHDRAW_AMOUNT_QUESTION, strlen(WITHDRAW_AMOUNT_QUESTION));
            if (write_bytes == -1)
            {
                perror("Error writing WITHDRAW_AMOUNT message to client!");
                unlock_critical_section(&sem_op);
                return false;
            }

            bzero(read_buffer, sizeof(read_buffer));
            read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer));
            if (read_bytes == -1)
            {
                perror("Error reading withdraw amount from client!");
                unlock_critical_section(&sem_op);
                return false;
            }

            withdraw_amount = atol(read_buffer);

            if (withdraw_amount != 0 && account.balance - withdraw_amount >= 0)
            {

                int new_transaction_id = write_transaction_to_file(account.account_number, account.balance, account.balance - withdraw_amount, 0);
                write_transaction_to_array(account.transactions, new_transaction_id);

                account.balance -= withdraw_amount;

                int account_file_descriptor = open("./db/ACCOUNT_FILE", O_WRONLY);
                off_t offset = lseek(account_file_descriptor, account.account_number * sizeof(struct Account), SEEK_SET);

                struct flock lock = {F_WRLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};
                int lockingStatus = fcntl(account_file_descriptor, F_SETLKW, &lock);
                if (lockingStatus == -1)
                {
                    perror("Error obtaining write lock on account record!");
                    unlock_critical_section(&sem_op);
                    return false;
                }

                write_bytes = write(account_file_descriptor, &account, sizeof(struct Account));
                if (write_bytes == -1)
                {
                    perror("Error writing updated balance into account file!");
                    unlock_critical_section(&sem_op);
                    return false;
                }

                lock.l_type = F_UNLCK;
                fcntl(account_file_descriptor, F_SETLK, &lock);

                write(connection_descriptor,WITHDRAW_AMOUNT_SUCCESS, strlen(WITHDRAW_AMOUNT_SUCCESS));
                read(connection_descriptor, read_buffer, sizeof(read_buffer)); // Dummy read

                get_balance(connection_descriptor);

                unlock_critical_section(&sem_op);

                return true;
            }
            else
                write_bytes = write(connection_descriptor, INVALID_WITHDRAW_AMOUNT, strlen(INVALID_WITHDRAW_AMOUNT));
        }
        else
            write(connection_descriptor, ACCOUNT_DEACTIVATED_MESSAGE, strlen(ACCOUNT_DEACTIVATED_MESSAGE));
        read(connection_descriptor, read_buffer, sizeof(read_buffer)); // Dummy read

        unlock_critical_section(&sem_op);
    }
    else
    {
        unlock_critical_section(&sem_op);
        return false;
    }
}

bool get_balance(int connection_descriptor)
{
    char buffer[1000];
    struct Account account;
    account.account_number = logged_in_user.account_number;
    if (get_account_details(connection_descriptor, &account))
    {
        bzero(buffer, sizeof(buffer));
        if (account.active_status)
        {
            sprintf(buffer, "You have ₹ %ld  money in your account!^", account.balance);
            write(connection_descriptor, buffer, strlen(buffer));
        }
        else
            write(connection_descriptor, ACCOUNT_DEACTIVATED_MESSAGE, strlen(ACCOUNT_DEACTIVATED_MESSAGE));
        read(connection_descriptor, buffer, sizeof(buffer)); // Dummy read
    }
    else
    {
        // ERROR while getting balance
        return false;
    }
}

bool change_password(int connection_descriptor)
{
    ssize_t read_bytes, write_bytes;
    char read_buffer[1000], write_buffer[1000];

    char newPassword[1000];

    // Lock the critical section
    struct sembuf sem_oper = {0, -1, SEM_UNDO};
    int sem_op_status = semop(sem_identifier, &sem_oper, 1);
    if (sem_op_status == -1)
    {
        perror("Error while locking critical section");
        return false;
    }

    write_bytes = write(connection_descriptor, OLD_PASS_REQUEST, strlen(OLD_PASS_REQUEST));
    if (write_bytes == -1)
    {
        perror("Error writing PASSWORD_CHANGE_OLD_PASS message to client!");
        unlock_critical_section(&sem_oper);
        return false;
    }

    bzero(read_buffer, sizeof(read_buffer));
    read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer));
    if (read_bytes == -1)
    {
        perror("Error reading old password response from client");
        unlock_critical_section(&sem_oper);
        return false;
    }

    if (strcmp(read_buffer, logged_in_user.password) == 0)
    {
        // Password matches with old password
        write_bytes = write(connection_descriptor, NEW_PASSWORD_REQUEST, strlen(NEW_PASSWORD_REQUEST));
        if (write_bytes == -1)
        {
            perror("Error writing PASSWORD_CHANGE_NEW_PASS message to client!");
            unlock_critical_section(&sem_oper);
            return false;
        }
        bzero(read_buffer, sizeof(read_buffer));
        read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer));
        if (read_bytes == -1)
        {
            perror("Error reading new password response from client");
            unlock_critical_section(&sem_oper);
            return false;
        }

        strcpy(newPassword, read_buffer);

        write_bytes = write(connection_descriptor, NEW_PASSWORD_RE_ENTER, strlen(NEW_PASSWORD_RE_ENTER));
        if (write_bytes == -1)
        {
            perror("Error writing PASSWORD_CHANGE_NEW_PASS_RE message to client!");
            unlock_critical_section(&sem_oper);
            return false;
        }
        bzero(read_buffer, sizeof(read_buffer));
        read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer));
        if (read_bytes == -1)
        {
            perror("Error reading new password reenter response from client");
            unlock_critical_section(&sem_oper);
            return false;
        }

        if (strcmp(read_buffer,newPassword) == 0)
        {
            // New & reentered passwords match

            strcpy(logged_in_user.password, newPassword);

            int customerFileDescriptor = open("./db/USER_FILE", O_WRONLY);
            if (customerFileDescriptor == -1)
            {
                perror("Error opening customer file!");
                unlock_critical_section(&sem_oper);
                return false;
            }

            off_t offset = lseek(customerFileDescriptor, logged_in_user.user_id * sizeof(struct User), SEEK_SET);
            if (offset == -1)
            {
                perror("Error seeking to the customer record!");
                unlock_critical_section(&sem_oper);
                return false;
            }

            struct flock lock = {F_WRLCK, SEEK_SET, offset, sizeof(struct User), getpid()};
            int lockingStatus = fcntl(customerFileDescriptor, F_SETLKW, &lock);
            if (lockingStatus == -1)
            {
                perror("Error obtaining write lock on customer record!");
                unlock_critical_section(&sem_oper);
                return false;
            }

            write_bytes = write(customerFileDescriptor, &logged_in_user, sizeof(struct User));
            if (write_bytes == -1)
            {
                perror("Error storing updated customer password into customer record!");
                unlock_critical_section(&sem_oper);
                return false;
            }

            lock.l_type = F_UNLCK;
            lockingStatus = fcntl(customerFileDescriptor, F_SETLK, &lock);

            close(customerFileDescriptor);

            write_bytes = write(connection_descriptor, PASSWORD_CHANGE_SUCCESS, strlen(PASSWORD_CHANGE_SUCCESS));
            read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer)); // Dummy read

            unlock_critical_section(&sem_oper);

            return true;
        }
        else
        {
            // New & reentered passwords don't match
            write_bytes = write(connection_descriptor, PASSWORD_DO_NOT_MATCH, strlen(PASSWORD_DO_NOT_MATCH));
            read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer)); // Dummy read
        }
    }
    else
    {
        // Password doesn't match with old password
        write_bytes = write(connection_descriptor, INVALID_OLD_PASSWORD, strlen(INVALID_OLD_PASSWORD));
        read_bytes = read(connection_descriptor, read_buffer, sizeof(read_buffer)); // Dummy read
    }

    unlock_critical_section(&sem_oper);

    return false;
}

bool lock_critical_section(struct sembuf *sem_oper)
{
    sem_oper->sem_flg = SEM_UNDO;
    sem_oper->sem_op = -1;
    sem_oper->sem_num = 0;
    int sem_op_status = semop(sem_identifier, sem_oper, 1);
    if (sem_op_status == -1)
    {
        perror("Error while locking critical section");
        return false;
    }
    return true;
}

bool unlock_critical_section(struct sembuf *sem_oper)
{
    sem_oper->sem_op = 1;
    int sem_op_status = semop(sem_identifier, sem_oper, 1);
    if (sem_op_status == -1)
    {
        perror("Error while operating on semaphore!");
        _exit(1);
    }
    return true;
}

void write_transaction_to_array(int *transaction_list, int transaction_id)
{
    int iter = 0;
    while (transaction_list[iter] != -1)
        iter++;

    if (iter >= MAX_TRANSACTIONS)
    {
        for (iter = 1; iter < MAX_TRANSACTIONS; iter++)
            // Shift elements one step back discarding the oldest transaction
            transaction_list[iter - 1] = transaction_list[iter];
        transaction_list[iter - 1] = transaction_id;
    }
    else
    {
        // Space available
        transaction_list[iter] = transaction_id;
    }
}

int write_transaction_to_file(int account_number, long int old_balance, long int new_balance, bool operation)
{
    struct Transaction new_transaction;
    new_transaction.account_number = account_number;
    new_transaction.old_balance = old_balance;
    new_transaction.new_balance = new_balance;
    new_transaction.operation = operation;
    new_transaction.transaction_time = time(NULL);

    ssize_t read_bytes, write_bytes;

    int transactionFileDescriptor = open("./db/TRANSACTION_FILE", O_CREAT | O_APPEND | O_RDWR, S_IRWXU);

    off_t offset = lseek(transactionFileDescriptor, -sizeof(struct Transaction), SEEK_END);
    if (offset >= 0)
    {
        // There exists at least one transaction record
        struct Transaction prevTransaction;
        read_bytes = read(transactionFileDescriptor, &prevTransaction, sizeof(struct Transaction));

        new_transaction.transaction_id = prevTransaction.transaction_id + 1;
    }
    else
        // No transaction records exist
        new_transaction.transaction_id = 0;

    write_bytes = write(transactionFileDescriptor, &new_transaction, sizeof(struct Transaction));

    return new_transaction.transaction_id;
}