#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <getopt.h>
#include <assert.h>

enum
{
    BUF_SIZE = 512,
    MODE_SIZE = 10
};

enum OPT_FLAG
{
    OPT_TRUE = 1,
    OPT_FALSE = 0
};

struct flags
{
    enum OPT_FLAG l;
    enum OPT_FLAG a;
    enum OPT_FLAG n;
    enum OPT_FLAG R;
    enum OPT_FLAG i;
    enum OPT_FLAG d;
};

struct ls
{
    char*  path;
    size_t path_size;
    char*  rec_buf;
    size_t rec_size;
    char*  link;
    size_t len;
    char*  datestr;
    char*  modestr;
    struct stat st;
    struct stat stl;
};

struct flags flag = {OPT_FALSE, OPT_FALSE, OPT_FALSE, OPT_FALSE, OPT_FALSE, OPT_FALSE};
char* progname;
struct ls* pls;

void parseFlags(int argc, char* argv[]);
void Ls(char* path);
void printShort(char* dirName, char* path);
void printLong(char* dirName, char* path);
void printName(struct stat* statBuf, char* path);
void recurceLs(char* dirName, char* path);
char* catPath(char* buff, char* dirName, char* path);
struct ls* lsInit();
void lsClear();
void cutStr(char*);


int main(int argc, char* argv[])
{
    progname = argv[0];
    parseFlags(argc, argv);

    pls = lsInit();

    if (argc == optind)
        Ls(".");
    else
    {
        for (int i = optind; i < argc; i++)
            Ls(argv[i]);
    }

    lsClear();

   return 0;
}


struct ls* lsInit()
{
    struct ls* ins = (struct ls*) malloc(sizeof(struct ls));

    ins->path = (char*) malloc(BUF_SIZE);
    ins->path_size = BUF_SIZE;
    ins->path[BUF_SIZE - 1] = '\0';
    ins->path[0] = '\0';

    ins->rec_buf = (char*) malloc(BUF_SIZE);
    ins->rec_size = BUF_SIZE;
    ins->rec_buf[BUF_SIZE - 1] = '\0';
    ins->rec_buf[0] = '\0';

    ins->link = (char*) malloc(BUF_SIZE);
    ins->link[BUF_SIZE - 1] = '\0';
    ins->link[0] = '\0';

    ins->datestr = (char*) malloc(BUF_SIZE);
    ins->datestr[BUF_SIZE - 1] = '\0';
    ins->modestr = (char*) malloc(MODE_SIZE);
    ins->modestr[MODE_SIZE - 1] = '\0';

    return ins;
}

void lsClear()
{
    if (pls->path)
        free(pls->path);
    if (pls->rec_buf)
        free(pls->rec_buf);
    if (pls->link)
        free(pls->link);
    if (pls->datestr)
        free(pls->datestr);
    if (pls->modestr)
        free(pls->modestr);
    free(pls);
    pls = NULL;
}


void Ls(char* path)
{
    assert(path);

    if (lstat(path, &(pls->st)))
    {
        fprintf(stderr, "%s: unable to access '%s': %s\n",
                progname, path, strerror(errno));
        return;
    }

    if (S_ISDIR(pls->st.st_mode) && !flag.d)
    {
        if (strcmp(path, "."))
            printf("%s:\n", path);

        DIR* d = opendir(path);
        if (d == NULL)
        {
            fprintf(stderr, "%s: unable to open directory '%s': %s\n",
                    progname, path, strerror(errno));
            return;
        }

        struct dirent* dir = NULL;

        while ((dir = readdir(d)))
        {
            if (dir->d_name[0] != '.' || flag.a)
            {
                if (flag.l || flag.n)
                {
                    printLong(path, dir->d_name);
                    printf("\n");
                }
                else
                {
                    printShort(path, dir->d_name);
                    printf("    ");
                }
            }
        }
        closedir(d);

        d = opendir(path);
        dir = NULL;
        while (flag.R && (dir = readdir(d)))
        {
            if ((dir->d_name[0] != '.' || flag.a) && 
                strcmp(".", dir->d_name) && strcmp("..", dir->d_name))
            {    
                recurceLs(path, dir->d_name); 
            }
        }
        closedir(d);
    }
    else
    {
        if (flag.l || flag.n)
        {
            printLong(NULL, path);
            printf("\n");
        }
        else
        {
            if (flag.i)
                printf("%ld ", (long) pls->st.st_ino);

            printName(&(pls->st), path);
        }
    }

    if (!flag.l && !flag.n)
        printf("\n");
}

void printLong(char* dirName, char* path)
{
    assert(path);
    
    if (!dirName)
        pls->path = path;
    else
    {
        pls->len = strlen(dirName) + strlen(path) + 2;
        if (pls->len > pls->path_size)
        {    
             pls->path = (char*) realloc(pls->path, pls->len * 2);
             pls->path_size = pls->len * 2;
             pls->path[pls->len - 1] = '\0';
        }
        catPath(pls->path, dirName, path);
    }

    if (lstat(pls->path, &(pls->st)))
    {

        fprintf(stderr, "%s: unable to access '%s': %s\n",
                progname, pls->path, strerror(errno));
        goto out;
    }
    
    if (flag.i)
        printf("%ld ", (long) pls->st.st_ino);

    if (S_ISDIR(pls->st.st_mode))
        printf("d");
    else if (S_ISCHR(pls->st.st_mode))
        printf("c");
    else if (S_ISBLK(pls->st.st_mode))
        printf("b");
    else if (S_ISFIFO(pls->st.st_mode))
        printf("p");
    else if (S_ISLNK(pls->st.st_mode))
        printf("l");
    else if (S_ISSOCK(pls->st.st_mode))
        printf("s");
    else
        printf("-");

    for (int j = 8; j >= 0; j--)
    {
        if (pls->st.st_mode & (1 << (8 - j)))
        {
            if (j % 3 == 0)
                pls->modestr[j] = 'r';
            else if (j % 3 == 1)
                pls->modestr[j] = 'w';
            else
                pls->modestr[j] = 'x';
        }
        else
        {
            pls->modestr[j] = '-';
        }
    }
    printf("%s %3ld ", pls->modestr, (long) pls->st.st_nlink);

    if (flag.n)
        printf("%d %d ", pls->st.st_uid, pls->st.st_gid);
    else
    {
        struct passwd* pwu = getpwuid(pls->st.st_uid);
        if (pwu == NULL)
            printf("%d ", pls->st.st_uid);
        else
            printf("%s ", pwu->pw_name);
        
        struct group* gr = getgrgid(pls->st.st_gid);
        if (gr == NULL)
            printf("%d ", pls->st.st_gid);
        else 
            printf("%s ", gr->gr_name);
    }

    printf("%7ld ", (long) pls->st.st_size);
    
    struct tm* time = localtime(&(pls->st.st_mtim.tv_sec));

    strftime(pls->datestr, BUF_SIZE - 1, "%Y %b %d %H:%M ", time);
    printf("%s", pls->datestr);

    if(!S_ISLNK(pls->st.st_mode))
    {
        printName(&(pls->st), path);
    }
    else
    {
        pls->len = readlink(pls->path, pls->link, BUF_SIZE - 1);
        if (pls->len == -1)
        {
            fprintf(stderr, "%s: unable to read symbolic link '%s': %s\n",
                    progname, pls->path, strerror(errno));
            goto out;
        }
        pls->link[pls->len] = '\0';
        
        if (stat(pls->path, &(pls->stl))) 
        {
            //link is broken
            printf("\x1b[1;31m%s\x1b[0m -> \x1b[1;31m%s\x1b[0m",
                   pls->path, pls->link);
        }
        else
        {
            printName(&(pls->st), path);
            printf(" -> ");
            printName(&(pls->stl), pls->link);
        }
    }
    
out:
    if (!dirName)
        pls->path = NULL;
}

void recurceLs(char* dirName, char* path)
{
    assert(dirName);
    assert(path);

    pls->len = strlen(dirName) + strlen(path) + 2;
    if (pls->len > pls->rec_size)
    {
        pls->rec_buf = (char*) realloc(pls->rec_buf, pls->len * 2);
        pls->rec_buf[pls->len - 1] = '\0';
        pls->rec_size = pls->len * 2;
    }
    catPath(pls->rec_buf, dirName, path);

    if (lstat(pls->rec_buf, &(pls->st)))
    {
        fprintf(stderr, "%s: unable to access '%s': %s\n", progname, pls->rec_buf, strerror(errno));
        return;
    }

    if (S_ISDIR(pls->st.st_mode) && flag.R)
    {
        printf("\n");
        Ls(pls->rec_buf);
    }
    cutStr(pls->rec_buf);
}

void printShort(char* dirName, char* path)
{
    pls->len = strlen(dirName) + strlen(path) + 2;
    if (pls->len > pls->path_size)
    {
        pls->path = (char*) realloc(pls->path, pls->len * 2);
        pls->path[pls->len - 1] = '\0';
        pls->path_size = pls->len * 2;
    }
    catPath(pls->path, dirName, path);

    if (lstat(pls->path, &(pls->st)))
    {
        fprintf(stderr, "%s: unable to access '%s': %s\n", progname, pls->path, strerror(errno));
        return;
    }
    if (flag.i)
        printf("%ld ", (long) pls->st.st_ino);

    printName(&(pls->st), path);
}

void printName(struct stat* statBuf, char* path)
{
    assert(statBuf);
    assert(path);

    if (S_ISDIR(statBuf->st_mode))
        printf("\x1b[1;34m%s\x1b[0m", path);

    else if (S_ISREG(statBuf->st_mode) && (statBuf->st_mode & (1 << 6)))   //executable file
        printf("\x1b[1;32m%s\x1b[0m", path);

    else if (S_ISLNK(statBuf->st_mode))
        printf("\x1b[1;36m%s\x1b[0m", path);

    else if (S_ISFIFO(statBuf->st_mode))
        printf("\x1b[0;33;40m%s\x1b[0m", path);

    else if (S_ISCHR(statBuf->st_mode) || S_ISBLK(statBuf->st_mode))
        printf("\x1b[1;33;40m%s\x1b[0m", path);

    else
        printf("%s", path);
}

char* catPath(char* buff, char* dirName, char* path)
{
    assert(buff);
    assert(dirName);
    assert(path);
    
    if (strcmp(buff, dirName))
    {
        buff[0] = '\0';
        strcat(buff, dirName);
    }
    strcat(buff, "/");
    strcat(buff, path);
    
    return buff;
}

void cutStr(char* str)
{
    assert(str);

    pls->len = strlen(str);
    char* tmp = str + pls->len - 1;
    while(*tmp != '/' && tmp != str)
        tmp--;
    
    if (*tmp == '/')
        *tmp = '\0';
}

void parseFlags(int argc, char* argv[])
{
    const char* optline = "lanRid";
    int ch = 0;
    struct option longopts[] = {
        {"all", no_argument, NULL, 'a'},
        {"directory", no_argument, NULL, 'd'},
        {"inode", no_argument, NULL, 'i'},
        {"numeric-uid-gid", no_argument, NULL, 'n'},
        {"recursive", no_argument, NULL, 'R'},
        {0, 0, 0, 0}
    };

    while ((ch = getopt_long(argc, argv, optline, longopts, NULL)) != -1)
    {
        switch (ch)
        {
            case 'l':
                flag.l = OPT_TRUE;
                break;
            case 'a':
                flag.a = OPT_TRUE;
                break;
            case 'n':
                flag.n = OPT_TRUE;
                break;
            case 'R':
                flag.R = OPT_TRUE;
                break;
            case 'i':
                flag.i = OPT_TRUE;
                break;
            case 'd':
                flag.d = OPT_TRUE;
                break;
        }
    }
}

