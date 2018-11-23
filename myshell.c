#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>


enum
{
    LINE_SIZE = 256,
    COMMAND_NUM = 16,
    ARG_NUM = 16
};

char*   progname;

char*** createBuf();
void    clearBuf();
int     strToTokens(char* str, char*** buf);
void    execute(char*** buf, const int size);


int main(int argc, char* argv[])
{
    progname = argv[0];
    size_t lineSize = LINE_SIZE;
    char* line = (char*) malloc(sizeof(char) * lineSize);
    char*** buf = createBuf(COMMAND_NUM);

    while (1)
    {
        printf("\x1b[1;32m#\x1b[0m ");

        int lineLen = getline(&line, &lineSize, stdin);

        if (lineLen < 0 || !strcmp(line, "exit\n") || !strcmp(line, "exit"))
        {
            printf("\n");
            break;
        }

        execute(buf, strToTokens(line, buf));
     }

    free(line);
    clearBuf(buf);

    return 0;
}


void execute(char*** buf, const int size)
{
    int pfd[2][2];

    for (int i = 0; i < size; i++)
    {
        int next = i % 2;
        int prev = (next + 1) % 2;

        if (size > 1)
            pipe(pfd[next]);

        pid_t pid = fork();
        if (pid == 0)
        {
            if (i != 0)
            {
                close(STDIN_FILENO);
                dup(pfd[prev][0]);
                close(pfd[prev][0]);
            }

            if (i != size - 1)
            {
                close(pfd[next][0]);
                close(STDOUT_FILENO);
                dup(pfd[next][1]);
                close(pfd[next][1]);
            }
            execvp(buf[i][0], buf[i]);

            fprintf(stderr, "%s: %s: Command not found\n", progname, buf[0][0]);
            exit(EXIT_FAILURE);
        }
        else
        {
            if (i != 0)
                close(pfd[prev][0]);
            if (i != size - 1)
                close(pfd[next][1]);
        }
    }  

    for (int i = 0; i < size; i++)
         wait(NULL);
}


int strToTokens(char* str, char*** buf)
{
    const char* delim1 = "|\n";
    const char* delim2 = " \t\r";

    buf[0][0] = strtok(str, delim1);
    for(int i = 1; (buf[i][0] = strtok(NULL, delim1)) && i < COMMAND_NUM; i++);

    for (int i = 0; i < COMMAND_NUM; i++)
    {
        buf[i][0] = strtok(buf[i][0], delim2);
        for (int j = 1; (buf[i][j] = strtok(NULL, delim2)) && j < ARG_NUM - 1; j++);
    }
    
    int realSize = 0;
    while (realSize < COMMAND_NUM && buf[realSize] && buf[realSize][0])
        realSize++;

    return realSize;
}


char*** createBuf()
{
    char*** buf = (char***) calloc(sizeof(char**), COMMAND_NUM);
    for (int i = 0; i < COMMAND_NUM; i++)
    {
        buf[i] = (char**) calloc(sizeof(char*), ARG_NUM);
    }

    return buf;
}


void clearBuf(char*** buf)
{
    for(int i = 0; i < COMMAND_NUM; i++)
        free(buf[i]);

    free(buf);
}

