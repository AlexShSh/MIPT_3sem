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

    pthread_cond_t  cond_cheese;
    pthread_cond_t  cond_ham;
    pthread_mutex_t mutex;

    int (*put_cheese)(struct monitor*);
    int (*put_ham)(struct monitor*);
    //int (*check_pizza)(struct monitor*);
    int (*is_ready)(struct monitor*);
};

char* progname;


void monitor_init(struct monitor*, const int);
void monitor_clear(struct monitor*);

void* cheeser(void* p);
void* hammer(void* p);
//void* checker(void* p);

int put_cheese(struct monitor*);
int put_ham(struct monitor*);
//int check_pizza(struct monitor*);
int is_ready(struct monitor*);


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

    pthread_t thrid[3];
    int err = 0;

    for (int i = 0; i < 2; i++)
    {
        err = pthread_create(thrid + i, NULL, hammer, &mon);
        error_check(err, "hammer create");
    }
    
    err = pthread_create(thrid + 2, NULL, cheeser, &mon);
    error_check(err, "cheeser create");

    for (int i = 0; i < 3; i++)
        pthread_join(thrid[i], NULL);
    
    for (int i = 0; i < npizzas; i++)
    {
        printf("%s\n", mon.pizzas[i].piz);
    }

    monitor_clear(&mon);

    return 0;
}


void monitor_init(struct monitor* mon, const int npizzas)
{
    assert(mon);

    mon->npizzas = npizzas;
    mon->nmade = 0;
    mon->pizzas = (struct pizza*) calloc(sizeof(struct pizza), npizzas);
    assert(mon->pizzas);

    //printf("!!!!!!!in  %p\n", mon->pizzas); 

    int err = pthread_mutex_init(&mon->mutex, NULL);
    error_check(err, "mutex init");
    err = pthread_cond_init(&mon->cond_cheese, NULL);
    error_check(err, "cond cheese init");
    err = pthread_cond_init(&mon->cond_ham, NULL);
    error_check(err, "cond ham init");

    mon->put_cheese = put_cheese;
    mon->put_ham = put_ham;
    mon->is_ready = is_ready;
}

void monitor_clear(struct monitor* mon)
{
    assert(mon);
    //printf("!!!!!!!out %p\n", mon->pizzas); 
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

int put_cheese(struct monitor* mon)
{
    assert(mon);

    int err = pthread_mutex_lock(&mon->mutex);
    error_check(err, "put cheese lock");
    
    if (mon->nmade < mon->npizzas)
    {
        if (mon->pizzas[mon->nmade].cheese == 1)
        {
            err = pthread_cond_wait(&mon->cond_cheese, &mon->mutex);
            error_check(err, "put cheese wait");
        }

        if (mon->nmade < mon->npizzas)
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
            }
        }
    }

    int res = mon->nmade != mon->npizzas;

    err = pthread_mutex_unlock(&mon->mutex);
    error_check(err, "put cheese unlock");

    return res;
}


int put_ham(struct monitor* mon)
{
    assert(mon);

    int err = pthread_mutex_lock(&mon->mutex);
    error_check(err, "put ham lock");
    
    if (mon->nmade < mon->npizzas)
    {
        while (mon->nmade < mon->npizzas && mon->pizzas[mon->nmade].ham == 2)
        {
            err = pthread_cond_wait(&mon->cond_ham, &mon->mutex);
            error_check(err, "put ham wait");
        }
 
        if (mon->nmade < mon->npizzas)
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
            }
        }
    }

    int res = mon->nmade != mon->npizzas;

    err = pthread_mutex_unlock(&mon->mutex);
    error_check(err, "put hum unlock");

    return res;
}

int is_ready(struct monitor* mon)
{
    assert(mon);

    int first = mon->pizzas[mon->nmade].i == 3;
    int second = (mon->pizzas[mon->nmade].cheese == 1) && (mon->pizzas[mon->nmade].ham == 2);
    assert(first == second);

    return first;
}
