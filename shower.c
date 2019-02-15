#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>


#define error_check(err, msg)                                        \
do {                                                                 \
    if (err)                                                         \
    {                                                                \
        fprintf(stderr, "%s: %s: %s", progname, msg, strerror(err)); \
        exit(EXIT_FAILURE);                                          \
    }                                                                \
} while(0)


enum shower_t
{
    FREE,
    MAN,
    WOMAN
};

struct monitor
{
    int cap;
    int load;
    enum shower_t show;

    pthread_mutex_t mutex;
    pthread_cond_t  cond_man;
    pthread_cond_t  cond_woman;
    pthread_cond_t  cond_full;
};


char* progname;


void monitor_init(struct monitor* mon, const int cap);
void monitor_clear(struct monitor* mon);

void* man(void* arg);
void* woman(void* arg);

void enter_man(struct monitor*);
void enter_woman(struct monitor*);
void exit_man(struct monitor*);
void exit_woman(struct monitor*);

int main(int argc, char* argv[])
{
    progname = argv[0];
    if (argc != 4)
    {
         printf("%s: wrong number of arguments: %d, expected: 3\n",
                progname, argc - 1);
         return 0;
    }

    const int nman = (int) strtol(argv[1], 0, 0);
    if (nman <= 0)
    {
         printf("%s: argument 1  must be positive\n", progname);
         return 0;
    }

    const int nwoman = (int) strtol(argv[2], 0, 0);
    if (nwoman <= 0)
    {
         printf("%s: argument 2 must be positive\n", progname);
         return 0;
    }

    const int cap = (int) strtol(argv[3], 0, 0);
    if (cap <= 0)
    {
         printf("%s: argument 3 must be positive\n", progname);
         return 0;
    }

    struct monitor mon;
    monitor_init(&mon, cap);

    pthread_t* thrid = (pthread_t*) malloc(sizeof(pthread_t) * (nman + nwoman));
    assert(thrid);
    int err = 0;

    for (int i = 0; i < nman; i++)
    {
        err = pthread_create(thrid + i, NULL, man, &mon);
        error_check(err, "man create");
    }

    for (int i = 0; i < nwoman; i++)
    {
        err = pthread_create(thrid + nman + i, NULL, woman, &mon);
        error_check(err, "woman create");
    }

    for (int i = 0; i < nman + nwoman; i++)
    {
        err = pthread_join(thrid[i], NULL);
        error_check(err, "join");
    }

    monitor_clear(&mon);
    free(thrid);
    
    return 0;
}


void* man(void* arg)
{
    assert(arg);
    struct monitor* mon = (struct monitor*) arg;

    enter_man(mon);
    printf("man enter\n");
    
    exit_man(mon);
    printf("man finish\n");

    return NULL;
}


void* woman(void* arg)
{
    assert(arg);
    struct monitor* mon = (struct monitor*) arg;

    enter_woman(mon);
    printf("woman enter\n");

    exit_woman(mon);
    printf("woman finish\n");

    return NULL;
}


void monitor_init(struct monitor* mon, const int cap)
{
    assert(mon);

    mon->cap = cap;
    mon->load = 0;
    mon->show = FREE;
    
    int err = pthread_mutex_init(&mon->mutex, NULL);
    error_check(err, "mutex init");
    err = pthread_cond_init(&mon->cond_man, NULL);
    error_check(err, "cond man init");
    err = pthread_cond_init(&mon->cond_woman, NULL);
    error_check(err, "cond woman init");
    err = pthread_cond_init(&mon->cond_full, NULL);
    error_check(err, "cond full init");
}


void monitor_clear(struct monitor* mon)
{
    assert(mon);

    int err = pthread_mutex_destroy(&mon->mutex);
    error_check(err, "mutex destroy");
    err = pthread_cond_destroy(&mon->cond_man);
    error_check(err, "cond man destroy");
    err = pthread_cond_destroy(&mon->cond_woman);
    error_check(err, "cond woman destroy");
    err = pthread_cond_destroy(&mon->cond_full);
    error_check(err, "cond full destroy");
}


void enter_man(struct monitor* mon)
{
    assert(mon);

    int err = pthread_mutex_lock(&mon->mutex);
    error_check(err, "enter man lock");
 
    while (1)
    {
        while (mon->show == WOMAN)
        {
            err = pthread_cond_wait(&mon->cond_man, &mon->mutex);
            error_check(err, "enter man wait");
        }
 
        while (mon->load == mon->cap)
        {
            err = pthread_cond_wait(&mon->cond_full, &mon->mutex);
            error_check(err, "enter man full wait");
        }
        if (mon->show == WOMAN)
            continue;

        mon->show = MAN;
        break;
    }

    mon->load++;

    err = pthread_mutex_unlock(&mon->mutex);
    error_check(err, "enter man unlock");
}


void exit_man(struct monitor* mon)
{
    assert(mon);

    int err = pthread_mutex_lock(&mon->mutex);
    error_check(err, "exit man lock");

    assert(mon->show == MAN);
 
    mon->load--;
    if (mon->load + 1 == mon->cap)
    {
        err = pthread_cond_broadcast(&mon->cond_full);
        error_check(err, "exit man full broadcast");
    }

    if (mon->load == 0)
    {
        mon->show = FREE;
        err = pthread_cond_broadcast(&mon->cond_woman);
        error_check(err, "exit man broadcast");
    }

    err = pthread_mutex_unlock(&mon->mutex);
    error_check(err, "exit man unlock");
}


void enter_woman(struct monitor* mon)
{
    assert(mon);

    int err = pthread_mutex_lock(&mon->mutex);
    error_check(err, "enter woman lock");

    while (1)
    {
        while (mon->show == MAN)
        {
            err = pthread_cond_wait(&mon->cond_woman, &mon->mutex);
            error_check(err, "enter woman wait");
        }
   
        while (mon->load == mon->cap)
        {
            err = pthread_cond_wait(&mon->cond_full, &mon->mutex);
            error_check(err, "enter man full wait");
        }

        if (mon->show == MAN)
            continue;

        mon->show = MAN;
        break;
    }

    mon->load++;
    
    err = pthread_mutex_unlock(&mon->mutex);
    error_check(err, "enter woman unlock");
}


void exit_woman(struct monitor* mon)
{
    assert(mon);

    int err = pthread_mutex_lock(&mon->mutex);
    error_check(err, "exit woman lock");

    assert(mon->show == WOMAN);

    mon->load--;
    if (mon->load + 1 == mon->cap)
    {
        err = pthread_cond_broadcast(&mon->cond_full);
        error_check(err, "exit woman full broadcast");
    }

    if (mon->load == 0)
    {
        mon->show = FREE;
        err = pthread_cond_broadcast(&mon->cond_man);
        error_check(err, "exit woman broadcast");
    }

    err = pthread_mutex_unlock(&mon->mutex);
    error_check(err, "exit woman unlock");
}

