#ifndef USER_RECORD
#define USER_RECORD
#include<time.h>
struct User
{
    int user_id;
    char user_name[30];
    int account_number;
    char login_id[40];
    char password[30];
    char gender;
    int age;
    time_t account_opening_date;
    // dob;

};
#endif