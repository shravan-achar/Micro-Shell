#include "parse.h"
#define WRITE_END 1
#define READ_END 0
int main (int argc, char *argv[]) {


pid_t pid;
int fd[2];

pipe(fd);
pid = fork();

if(pid==0)
{

    dup2(fd[WRITE_END], 1);
    close(fd[READ_END]);
    close(fd[WRITE_END]);
    execvp("ls", argv);
}
else
{ 
    pid=fork();

    if(pid==0)
    {

        dup2(fd[READ_END], 0);
        close(fd[WRITE_END]);
        close(fd[READ_END]);
        execvp("wc", argv);
    }
    else
    {
        int status;
        close(fd[READ_END]);
        close(fd[WRITE_END]);
        waitpid(pid, &status, 0);
    }
}
return 0;
}
