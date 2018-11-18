#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <string.h>


#define ASSERT(MSG, EXPR)                                                  \
do{                                                                        \
    if (!(EXPR))                                                           \
    {                                                                      \
        fprintf(stderr, "%s: '"MSG"': %s\n", progname, strerror(errno));   \
        exit(EXIT_FAILURE);                                                \
    }                                                                      \
} while(0);


enum
{
    ARGC = 5,
};

enum SEM
{
    SHIP = 0,
    LADD,
    END
};

/*  sem[SHIP] - number of available seats on ship
    sem[LADD] - number of available seats on ladder
    sem[END]  - used to keep passengers from leaving the ship
                before departure
*/

char* progname;
int   semid;

void passenger(const long i);
void ship(const long ship_cur, const long ladd_cap, const long nfloat);

void init_ship();
void open_ladd(const long);
void close_ladd(const long);
void go_ship();
void leave_ship();
void enjoy();



int main(int argc, char* argv[])
{
    progname = argv[0];

    if (argc != ARGC)
    {
        printf("%s: wrong number of arguments: %d, expected: %d\n",
               progname, argc - 1, ARGC - 1);
        return 0;
    }
    
    const long npass = strtol(argv[1], 0, 0);
    if (npass <= 0)
    {
        printf("%s: invalid argument 1: %ld, expected positive number\n",
               progname, npass);
        return 0;
    }
    
    const long ship_cap = strtol(argv[2], 0, 0);
    if (ship_cap <= 0)
    {
        printf("%s: invalid argument 2: %ld, expected positive number\n",
               progname, ship_cap);
        return 0;
    }

    const long ladd_cap = strtol(argv[3], 0, 0);
    if (ladd_cap <= 0)
    {
        printf("%s: invalid argument 3: %ld, expected positive number\n",
               progname, ladd_cap);
        return 0;
    }

    const long nfloat = strtol(argv[4], 0 ,0);
    if (nfloat <= 0)
    {
        printf("%s: invalid argument 4: %ld, expected positive number\n",
               progname, nfloat);
        return 0;
    }

    semid = semget(IPC_PRIVATE, 3, 0700);
    ASSERT("semget", semid != -1);
    

    if (fork() == 0)
        ship(ship_cap, ladd_cap, nfloat); 

    for (long i = 0; i < npass; i++)
    {
        if (fork() == 0)
            passenger(i);
    }
    
    for (long i = 0; i < npass + 1; i++)
        wait(NULL);

    int ctl = semctl(semid, 0, IPC_RMID, 0);
    ASSERT("semctl", ctl != -1);

    printf("Success\n");

    return 0;
}

void ship(const long ship_cap, const long ladd_cap, const long nfloat)
{
    init_ship(ship_cap);

    for (long i = 0; i < nfloat; i++)
    {
        printf("Ship: sailed to beach\n");

        open_ladd(ladd_cap);
        printf("Ship: open ladder\n");
        usleep(100000);

        close_ladd(ladd_cap);
        printf("Ship: close ladder\nShip: depart\n");
        usleep(100000);
    }

    exit(EXIT_SUCCESS);
}

void passenger(const long i)
{
    while(1)
    {
        printf("Passenger %2ld: want to go to ship\n", i);
        go_ship();

        printf("Passenger %2ld: went on ship\n", i);
        enjoy();

        leave_ship();
        printf("Passenger %2ld: leave the ship\n", i);
    }
    exit(EXIT_SUCCESS);
}


void init_ship(const long ship_cap)
{
    struct sembuf init = {SHIP, ship_cap, 0};
    int res = semop(semid, &init, 1);
    ASSERT("semop", res != -1);
}

void open_ladd(const long ladd_cap)
{
    //forbid to leave the ship before departure
    struct sembuf forbid = {END, 1, 0};
    int res = semop(semid, &forbid, 1);
    ASSERT("semop", res != -1);
    
    //open ladder
    struct sembuf open = {LADD, ladd_cap, 0};
    res = semop(semid, &open, 1); 
    ASSERT("semop", res != -1);
}

void close_ladd(const long ladd_cap)
{
    //close ladder
    struct sembuf close = {LADD, -ladd_cap, 0};
    int res = semop(semid, &close, 1);
    ASSERT("semop", res != -1);

    //allow to leave the ship
    struct sembuf allow = {END, -1, 0};
    res = semop(semid, &allow, 1);
    ASSERT("semop", res != -1);
}


void go_ship()
{
    //buy a ticket
    struct sembuf ticket = {SHIP, -1, 0};
    int res = semop(semid, &ticket, 1);
    ASSERT("semop", res != -1);

    //go to ladder
    struct sembuf ladd = {LADD, -1, 0};
    res = semop(semid, &ladd, 1);
    ASSERT("semop", res != -1);

    //go to ship
    struct sembuf ship = {LADD, 1, 0};
    res = semop(semid, &ship, 1);
    ASSERT("semop", res != -1);
}


void enjoy()
{
    struct sembuf waiting = {END, 0, 0};
    int res = semop(semid, &waiting, 1);
    ASSERT("semop", res != -1);
}

void leave_ship()
{
    //go to ladder
    struct sembuf ladd = {LADD, -1, 0};
    int res = semop(semid, &ladd, 1);
    ASSERT("semop", res != -1);

    //free seat on the ship
    struct sembuf leave = {SHIP, 1, 0};
    res = semop(semid, &leave, 1);
    ASSERT("semop", res != -1);
    
    //go to beach
    struct sembuf beach = {LADD, 1, 0};
    res = semop(semid, &beach, 1);
    ASSERT("semop", res != -1);
}




