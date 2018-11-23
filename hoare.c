#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>


#define error_check(err, msg)                                        \
do {                                                                 \
    if (err)                                                         \
    {                                                                \
        fprintf(stderr, "%s: %s: %s", progname, msg, strerror(err)); \
        exit(EXIT_FAILURE);                                          \
    }                                                                \
} while(0)
        

struct pizza
{
    char piz[4];
    int  i;
    int  cheese;
    int  ham;
};

struct monitor
{
    struct pizza* pizzas;
    int           npizzas;
    int           nmade;
    int           ncheck;

    pthread_cond_t  cond_cheese;
    pthread_cond_t  cond_ham;
    pthread_cond_t  cond_check;
    pthread_mutex_t mutex;

    int (*put_cheese)(struct monitor*);
    int (*put_ham)(struct monitor*);
    int (*is_ready)(struct monitor*);
    int (*check_pizza)(struct monitor*);
    int (*all_ready)(struct monitor*);
    int (*all_check)(struct monitor*);
};

char* progname;


void monitor_init(struct monitor*, const int);
void monitor_clear(struct monitor*);

void* cheeser(void* arg);
void* hammer (void* arg);
void* checker(void* arg);

int put_cheese(struct monitor*);
int put_ham(struct monitor*);
int check_pizza(struct monitor*);
int is_ready(struct monitor*);
int all_ready(struct monitor*);
int all_check(struct monitor*);


int main(int argc, char* argv[])
{
    progname = argv[0];
    if (argc != 2)
    {
        printf("%s: wrong number of arguments: %d, expected: 1\n",
               progname, argc - 1);
        return 0;
    }

    const int npizzas = (int) strtol(argv[1], 0, 0);
    if (npizzas <= 0)
    {
        printf("%s: argument must be positive\n", progname);
        return 0;
    }

    struct monitor mon;
    monitor_init(&mon, npizzas);

    pthread_t thrid[4];
    int err = 0;

    for (int i = 0; i < 2; i++)
    {
        err = pthread_create(thrid + i, NULL, hammer, &mon);
        error_check(err, "hammer create");
    }
    
    err = pthread_create(thrid + 2, NULL, cheeser, &mon);
    error_check(err, "cheeser create");

    err = pthread_create(thrid + 3, NULL, checker, &mon);
    error_check(err, "checker create");

    for (int i = 0; i < 4; i++)
        pthread_join(thrid[i], NULL);
    
    monitor_clear(&mon);

    return 0;
}


void monitor_init(struct monitor* mon, const int npizzas)
{
    assert(mon);

    mon->npizzas = npizzas;
    mon->nmade = 0;
    mon->ncheck = 0;
    mon->pizzas = (struct pizza*) calloc(sizeof(struct pizza), npizzas);
    assert(mon->pizzas);

    int err = pthread_mutex_init(&mon->mutex, NULL);
    error_check(err, "mutex init");
    err = pthread_cond_init(&mon->cond_cheese, NULL);
    error_check(err, "cond cheese init");
    err = pthread_cond_init(&mon->cond_ham, NULL);
    error_check(err, "cond ham init");
    err = pthread_cond_init(&mon->cond_check, NULL);
    error_check(err, "cond check init");

    mon->put_cheese = put_cheese;
    mon->put_ham = put_ham;
    mon->is_ready = is_ready;
    mon->check_pizza = check_pizza;
    mon->all_ready = all_ready;
    mon->all_check = all_check;
}


void monitor_clear(struct monitor* mon)
{
    assert(mon);
    free(mon->pizzas);

    int err = pthread_mutex_destroy(&mon->mutex);
    error_check(err, "mutex destroy");
    err = pthread_cond_destroy(&mon->cond_cheese);
    error_check(err, "cond cheese destroy");
    err = pthread_cond_destroy(&mon->cond_ham);
    error_check(err, "cond ham destroy");
}


void* cheeser(void* arg)
{
    assert(arg);

    struct monitor* mon = (struct monitor*) arg;
    while (mon->put_cheese(mon));

    return NULL;
}


void* hammer(void* arg)
{
    assert(arg);

    struct monitor* mon = (struct monitor*) arg;
    while (mon->put_ham(mon));

    return NULL;
}


void* checker(void* arg)
{
    assert(arg);

    struct monitor* mon = (struct monitor*) arg;
    while (mon->check_pizza(mon));

    return NULL;
}


int put_cheese(struct monitor* mon)
{
    assert(mon);

    int err = pthread_mutex_lock(&mon->mutex);
    error_check(err, "put cheese lock");
    
    if (!mon->all_ready(mon) && mon->pizzas[mon->nmade].cheese == 1)
    {
        err = pthread_cond_wait(&mon->cond_cheese, &mon->mutex);
        error_check(err, "put cheese wait");
    }

    if (!mon->all_ready(mon))
    {
        int i = mon->pizzas[mon->nmade].i;
        mon->pizzas[mon->nmade].piz[i] = 'c';
        mon->pizzas[mon->nmade].i++;
        mon->pizzas[mon->nmade].cheese++;

        if (mon->is_ready(mon))
        {
            mon->nmade++;
            err = pthread_cond_broadcast(&mon->cond_ham);
            error_check(err, "put cheese broadcast");

            err = pthread_cond_signal(&mon->cond_check);
            error_check(err, "put cheese signal check");
        }
    }

    int res = !mon->all_ready(mon);

    err = pthread_mutex_unlock(&mon->mutex);
    error_check(err, "put cheese unlock");

    return res;
}


int put_ham(struct monitor* mon)
{
    assert(mon);

    int err = pthread_mutex_lock(&mon->mutex);
    error_check(err, "put ham lock");
    
    while (!mon->all_ready(mon) && mon->pizzas[mon->nmade].ham == 2)
    {
        err = pthread_cond_wait(&mon->cond_ham, &mon->mutex);
        error_check(err, "put ham wait");
    }

    if (!mon->all_ready(mon))
    {
        int i = mon->pizzas[mon->nmade].i;
        mon->pizzas[mon->nmade].piz[i] = 'h';
        mon->pizzas[mon->nmade].i++;
        mon->pizzas[mon->nmade].ham++;
    
        if (mon->is_ready(mon))
        {
            mon->nmade++;
            err = pthread_cond_signal(&mon->cond_cheese);
            error_check(err, "put ham signal");

            err = pthread_cond_signal(&mon->cond_check);
            error_check(err, "put ham signal check");
        }
    }
    
    int res = !mon->all_ready(mon);

    err = pthread_mutex_unlock(&mon->mutex);
    error_check(err, "put hum unlock");

    return res;
}


int check_pizza(struct monitor* mon)
{
    assert(mon);

    int err = pthread_mutex_lock(&mon->mutex);
    error_check(err, "check pizza lock");

    if (!mon->all_check(mon) && mon->ncheck == mon->nmade)
    {
        err = pthread_cond_wait(&mon->cond_check, &mon->mutex);
        error_check(err, "check wait");
    }
    
    if (!mon->all_check(mon))
    {
        int i = mon->pizzas[mon->ncheck].i;
        int icond = i == 3;
        if (!icond)
            printf("Pizza %d is bad! It has %d components\n", 
                   mon->ncheck, i);

        int cheese = mon->pizzas[mon->ncheck].cheese;
        int ccond = cheese == 1;
        if (!ccond)
            printf("Pizza %d is bad! it has %d cheeses\n", 
                   mon->ncheck, cheese);

        int ham = mon->pizzas[mon->ncheck].ham;
        int hcond = ham == 2;
        if (!ccond)
            printf("Pizza %d is bad! it has %d hams\n", 
                   mon->ncheck, ham);

        int ccount = 0;
        int hcount = 0;
        for (int j = 0; j < 3; j++)
        {
            char c = mon->pizzas[mon->ncheck].piz[j];
            if (c == 'c')
                ccount++;
            else if (c == 'h')
                hcount++;  
        }
        int strcond = (ccount == 1) && (hcount == 2);
        if (!strcond)
            printf("Pizza %d: \"%s\" is bad!\n",
                   mon->ncheck, mon->pizzas[mon->ncheck].piz);
        
        if (icond && ccond && hcond && strcond)
            printf("Pizza %d: \"%s\" is good!\n",
                   mon->ncheck, mon->pizzas[mon->ncheck].piz);

        mon->ncheck++;
    }

    int res = !mon->all_check(mon);

    err = pthread_mutex_unlock(&mon->mutex);
    error_check(err, "check unlock");

    return res;
}


int is_ready(struct monitor* mon)
{
    assert(mon);
    return mon->pizzas[mon->nmade].i == 3;
}


int all_ready(struct monitor* mon)
{
    assert(mon);
    return mon->nmade == mon->npizzas; 
}


int all_check(struct monitor* mon)
{
    assert(mon);
    return mon->ncheck == mon->npizzas;
}

