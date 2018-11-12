#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <sys/time.h>


enum MSG_TYPE
{
    READY = 1,
    START = 10,  //type of message "start to runner i" = START + i
    FINISH = 2
};

struct mymsgbuf
{
    long type;
    int num;
};

int msgid = 0;
long nrunner = 0;

void runner(int i);
void judge();
double getCurrentTime();


int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        printf("Wrong number of arguments: %d, expected: 1\n", argc - 1);
        return 0;
    }

    nrunner = strtol(argv[1], 0 , 0);
    if (nrunner <= 0)
    {
        printf("Number of runners must de positive\n");
        return 0;
    }

    if ((msgid = msgget(IPC_PRIVATE, 0700)) == -1)
    {
        perror("Cant get msgid");
        exit(-1);
    }

    pid_t pid = fork();
    if (pid == 0)
        judge();

    for (int i = 0; i < nrunner; i++)
    {
        pid = fork();
        if (pid == 0)
            runner(i);
    }

    for (int i = 0; i < nrunner + 1; i++)
        wait(NULL);

    if (msgctl(msgid, IPC_RMID, NULL) < 0)
    {
        perror("Cant remove msg");
    }

    return 0;
}


void judge()
{
    printf("Judge: arrived at the stadium\n");

    for (int i = 0; i < nrunner; i++)
    {
        struct mymsgbuf ready;
        if (msgrcv(msgid, (struct msgbuf*) &ready, sizeof(struct mymsgbuf) - sizeof(long), (long) READY, 0) < 0)
        {
            perror("Judge can't receive message \"Ready\"\n");
            exit(EXIT_FAILURE);
        }
        printf("Judge: runner %d is ready to start\n", ready.num);
    }

    printf("Judge: START!\n");

    double startRaceTm = getCurrentTime();

    struct mymsgbuf start = {(long) START, 0};
    if (msgsnd(msgid, (struct msgbuf*) &start, sizeof(struct mymsgbuf) - sizeof(long), 0) < 0)
    {
        fprintf(stderr, "Judge can't send message \"Start\": %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    struct mymsgbuf finish;
    if (msgrcv(msgid, (struct msgbuf*) &finish, sizeof(struct mymsgbuf) - sizeof(long), (long) FINISH, 0) < 0)
    {
        fprintf(stderr, "Judge can't receive message \"Finish\": %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (finish.num != nrunner - 1)
    {
        fprintf(stderr, "Violation of the order runners\n");
        exit(EXIT_FAILURE);
    }

    double finishRaceTm = getCurrentTime();
    printf("Judge: race finish, total time: %.2lf\n", finishRaceTm - startRaceTm);

    exit(EXIT_SUCCESS);
}

void runner(int i)
{
    printf("Runner %d: arrived at the stadium\n", i);

    struct mymsgbuf ready = {(long) READY, i};
    if (msgsnd(msgid, (struct msgbuf*) &ready, sizeof(struct mymsgbuf) - sizeof(long), 0) < 0)
    {
        fprintf(stderr, "Runner %d can't send message \"Ready\": %s\n", i, strerror(errno));
        exit(EXIT_FAILURE);
    }

    struct mymsgbuf start;
    if (msgrcv(msgid, (struct msgbuf*) &start, sizeof(struct mymsgbuf) - sizeof(long), (long) START + i, 0) < 0)
    {
        fprintf(stderr, "Runner %d can't receive message \"Start\": %s\n", i, strerror(errno));
        exit(EXIT_FAILURE);
    }
    printf("Runner %d: start\n", i);

    srand(time(NULL) + getpid());
    usleep((rand() % 100) * 10000);

    printf("Runner %d: finish\n", i);

    if (i != nrunner - 1)
    {
        struct mymsgbuf start = {START + i + 1, 0};
        if (msgsnd(msgid, (struct msgbuf*) &start, sizeof(struct mymsgbuf) - sizeof(long), 0) < 0)
        {
            fprintf(stderr, "Runner %d can't send message \"Start %d\": %s\n", i, i + 1, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        struct mymsgbuf finish = {FINISH, i};
        if (msgsnd(msgid, (struct msgbuf*) &finish, sizeof(struct mymsgbuf) - sizeof(long), 0) < 0)
        {
            fprintf(stderr, "Runner %d can't send message \"Finish\": %s\n", i, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    exit(EXIT_SUCCESS);
}

double getCurrentTime()
{
    struct timeval tv = {0, 0};
    gettimeofday(&tv, NULL);

    return tv.tv_sec + (double) (tv.tv_usec) / 1000000;
}
