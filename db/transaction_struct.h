#ifndef TRANSACTIONS
#define TRANSACTIONS

#include <time.h>

struct Transaction
{
    int transaction_id; // 0, 1, 2, 3 ...
    int account_number;
    bool operation; // 0 -> Withdraw, 1 -> Deposit
    long int old_balance;
    long int new_balance;
    time_t transaction_time;
};

#endif