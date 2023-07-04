#ifndef ACCOUNT_RECORD
#define ACCOUNT_RECORD
#define MAX_TRANSACTIONS 20
struct Account
{
    int account_number;
    int owners[2];
    int account_type;
    bool active_status;
    long int balance;
     int transactions[MAX_TRANSACTIONS];
};
#endif