#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>


#define error_check(MSG, res)                \
do{                                          \
    if (res == -1)                           \
    {                                        \
        fprintf(stderr, "%s: '"MSG"': %s\n", \
                progname, strerror(errno));  \
        exit(EXIT_FAILURE);                  \
    }                                        \
} while(0)

enum
{
    BUF_SIZE = 256,
};

char* progname;

pid_t rpid = -1;
pid_t wpid = -1;

int wait_ans = 1;
int wait_bit = 1;

int bit = 0;

sigset_t oldmask;


void reader();
void writer();

void init_reader();
void init_writer();

void handler_wr(int sig);
void handler_rd(int sig);

void send_byte(char byte);
char recv_byte();


int main(int argc, char* argv[])
{
    progname = argv[0];
    if (argc != 1)
    {
        printf("%s: arguments are not expected\n",
               progname);
        return 0;
    }

    wpid = getpid();
    rpid = fork();
    error_check("fork", rpid);

    if (rpid == 0)
        reader();
    else
        writer();

    wait(NULL);
    wait(NULL);

    return 0;
}


void writer()
{
    init_writer();
    int res = -1;
    char byte = -1;
    while ((byte = recv_byte()) != EOF)
    {
        res = write(STDOUT_FILENO, &byte, 1);
        error_check("write", res);
    }

    exit(EXIT_SUCCESS);
}


void reader()
{
    init_reader();
    int read_res = 0;
    char buf[BUF_SIZE + 1];

    while ((read_res = read(STDIN_FILENO, buf, BUF_SIZE)))
    {
        error_check("read", read_res);
        for (int i = 0; i < read_res; i++)
        {
            send_byte(buf[i]);
        }
    }
    send_byte(EOF);

    exit(EXIT_SUCCESS);
}


void send_byte(char byte)
{
    int res = -1;
    for (int i = 0; i < 8; i++)
    {
        wait_ans = 1;

        bit = (1 << i) & byte;
        int sig = (bit == 0) ? SIGUSR1 : SIGUSR2;
        res = kill(wpid, sig);
        error_check("rd kill", res);
 
        while (wait_ans)
        {
            sigsuspend(&oldmask);
            if (errno != EINTR)
                error_check("rd sigsuspend", -1);
        }
    }
}


char recv_byte()
{
    int res = -1;
    char byte = 0;
    for (int i = 0; i < 8; i++)
    {
        wait_bit = 1;
        while (wait_bit)
        {
            sigsuspend(&oldmask);
            if (errno != EINTR)
                error_check("rd sigsuspend", -1);
        }
        byte = byte | (bit << i);

        res = kill(rpid, SIGUSR1);
        error_check("wr kill", res);
    }
    return byte;
}


void handler_wr(int sig)
{
    switch (sig)
    {
        case SIGUSR1:
            bit = 0;
            break;
        case SIGUSR2:
            bit = 1;
            break;
        default:
            fprintf(stderr, "%s: handler wr: unknown signal: %d\n",
                    progname, sig);
            exit(EXIT_FAILURE);
    }
    wait_bit = 0;
}


void handler_rd(int sig)
{
    if (sig != SIGUSR1)
    {
        fprintf(stderr, "%s: handler rd: unknown signal: %d\n",
                progname, sig);
        exit(EXIT_FAILURE);
    }
    wait_ans = 0;
}


void init_writer()
{
    int res = -1;

    sigset_t mask;
    res = sigemptyset(&mask);
    error_check("wr sigemptyset", res);
    res = sigaddset(&mask, SIGUSR1);
    error_check("wr sigaddset usr1", res);
    res = sigaddset(&mask, SIGUSR2);
    error_check("wr sigaddset usr2", res);
    res = sigprocmask(SIG_BLOCK, &mask, &oldmask);
    error_check("wr sigprocmask", res);
    
    struct sigaction act = {};
    act.sa_handler = handler_wr;
    res = sigaction(SIGUSR1, &act, NULL);
    error_check("wr sigaction usr1", res);
    res = sigaction(SIGUSR2, &act, NULL);
    error_check("wr sigaction usr2", res);
}


void init_reader()
{
    int res = -1;

    sigset_t mask;
    res = sigemptyset(&mask);
    error_check("rd sigemptyset", res);
    res = sigaddset(&mask, SIGUSR1);
    error_check("rd sigaddset usr1", res);
    res = sigprocmask(SIG_BLOCK, &mask, &oldmask);
    error_check("rd sigprocmask", res);
    
    struct sigaction act = {};
    act.sa_handler = handler_rd;
    act.sa_mask = mask;
    res = sigaction(SIGUSR1, &act, NULL);
    error_check("rd sigaction usr1", res);
}

