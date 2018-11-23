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
    SLEEP,
    END
};

/*  sem[SHIP]   - number of available seats on ship
    sem[LADD]   - number of available seats on ladder
    sem[SLEEP]  - used to keep passengers from leaving the ship
                  before departure
    sem[END}    - used to finish program
*/

char* progname;
int   semid;

void ship(const short ship_cur, const short ladd_cap, const short nfloat);
void init_ship(const short);
void open_ladd(const short);
void end_cruise(const short, const short);
void close_ladd(const short);

void passenger(const short i);
void buy_ticket();
int  check_end();
void return_ticket();
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
    
    const short npass = (short) strtol(argv[1], 0, 0);
    if (npass <= 0)
    {
        printf("%s: invalid argument 1: %d, expected number in range from 1 to 32767\n",
               progname, npass);
        return 0;
    }
    
    const short ship_cap = (short) strtol(argv[2], 0, 0);
    if (ship_cap <= 0)
    {
        printf("%s: invalid argument 2: %d, expected number in range from 1 to 32767\n",
               progname, ship_cap);
        return 0;
    }

    const short ladd_cap = (short) strtol(argv[3], 0, 0);
    if (ladd_cap <= 0)
    {
        printf("%s: invalid argument 3: %d, expected number in range from 1 to 32767\n",
               progname, ladd_cap);
        return 0;
    }

    const short nfloat = (short) strtol(argv[4], 0 ,0);
    if (nfloat <= 0)
    {
        printf("%s: invalid argument 4: %d, expected number in range from 1 to 32767\n",
               progname, nfloat);
        return 0;
    }

    semid = semget(IPC_PRIVATE, 4, 0700);
    ASSERT("semget", semid != -1);

    if (fork() == 0)
        ship(ship_cap, ladd_cap, nfloat); 

    for (short i = 0; i < npass; i++)
    {
        if (fork() == 0)
            passenger(i);
    }
    
    for (short i = 0; i < npass + 1; i++)
        wait(NULL);

    int ctl = semctl(semid, 0, IPC_RMID, 0);
    ASSERT("semctl", ctl != -1);

    printf("Success\n");

    return 0;
}


void ship(const short ship_cap, const short ladd_cap, const short nfloat)
{
    init_ship(ship_cap);

    for (short i = 0; i < nfloat; i++)
    {
        printf("Ship: sailed to beach\n");

        open_ladd(ladd_cap);
        usleep(100000);

        close_ladd(ladd_cap);
        usleep(100000);
    }

    end_cruise(ship_cap, ladd_cap);

    printf("Ship: leave beach\n");

    exit(EXIT_SUCCESS);
}


void passenger(const short i)
{
    while(1)
    {
        printf("Passenger %2d: want to go to ship\n", i);
        buy_ticket();
        if (check_end())
            break;
             
        go_ship();
        printf("Passenger %2d: went on ship\n", i);

        enjoy();

        printf("Passenger %2d: leave the ship\n", i);
        leave_ship();
    }

    return_ticket();
    printf("Passenger %2d: leave the beach\n", i);

    exit(EXIT_SUCCESS);
}


void init_ship(const short ship_cap)
{
    struct sembuf init = {SHIP, ship_cap, 0};
    int res = semop(semid, &init, 1);
    ASSERT("semop", res != -1);
}


void open_ladd(const short ladd_cap)
{
    //forbid to leave the ship before departure
    struct sembuf forbid = {SLEEP, 1, 0};
    int res = semop(semid, &forbid, 1);
    ASSERT("semop", res != -1);
    
    //open ladder
    printf("Ship: open ladder\n");
    struct sembuf open = {LADD, ladd_cap, 0};
    res = semop(semid, &open, 1); 
    ASSERT("semop", res != -1);
}


void close_ladd(const short ladd_cap)
{
    //close ladder
    printf("Ship: close ladder\n");
    struct sembuf close = {LADD, -ladd_cap, 0};
    int res = semop(semid, &close, 1);
    ASSERT("semop", res != -1);

    //allow to leave the ship
    struct sembuf allow = {SLEEP, -1, 0};
    res = semop(semid, &allow, 1);
    ASSERT("semop", res != -1);
}


void end_cruise(const short ship_cap, const short ladd_cap)
{
    //signal to passangers that cruise end
    struct sembuf end = {END, 1, 0};
    int res = semop(semid, &end, 1);
    ASSERT("semop", res != -1);

    open_ladd(ladd_cap);

    //wait when all passangers leave ship
    struct sembuf wait = {SHIP, ship_cap, 0};
    res = semop(semid, &wait, 1);
    ASSERT("semop", res != -1);
}


void buy_ticket()
{
    struct sembuf ticket = {SHIP, -1, 0};
    int res = semop(semid, &ticket, 1);
    ASSERT("semop", res != -1);
}


int check_end()
{
    struct sembuf check = {END, 0, IPC_NOWAIT};
    int res = semop(semid, &check, 1);
    if (res == -1 && errno == EAGAIN)
        return 1;

    ASSERT("semop", res != -1);
    return 0;
}


void go_ship()
{
    //go to ladder
    struct sembuf ladd = {LADD, -1, 0};
    int res = semop(semid, &ladd, 1);
    ASSERT("semop", res != -1);

    //go to ship
    struct sembuf ship = {LADD, 1, 0};
    res = semop(semid, &ship, 1);
    ASSERT("semop", res != -1);
}


void return_ticket()
{
    struct sembuf ret = {SHIP, 1, 0};
    int res = semop(semid, &ret, 1);
    ASSERT("semop", res != -1);
}


void enjoy()
{
    struct sembuf waiting = {SLEEP, 0, 0};
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

