/******************************************************************************
 *
 *  File Name........: main.c
 *
 *  Description......: Simple driver program for ush's parser
 *
 *  Author...........: Vincent W. Freeh
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include "parse.h"

static void handle_exec_error() 
{
    switch(errno) {

	case EPERM: 
	    fprintf(stderr, "permission denied\n");
	    break;
	case ENOENT:
	    fprintf(stderr, "command not found\n");
	    break;
    default:
        fprintf(stderr, "%s\n", strerror(errno));
        break;
    }

    _exit(EXIT_FAILURE);
}

static void manageio(Cmd c, int * prev, int * next, Job j)
{

    int flags;
    switch (c->in) {
        case Tpipe:
        case TpipeErr:
            dup2(prev[0], 0);
            close(prev[0]);
            close(prev[1]);
            break;
        case Tin:
            flags = O_RDONLY;
            fileRedir(c, flags);
            break;
        default:
            break;

    }

    switch (c->out) { 
        case Tnil: 
            if (execvp(c->args[0], c->args) < 0) handle_exec_error();
            break;

        case Tpipe:
        case TpipeErr:
            {
                dup2(next[1], 1);
                if (c->out == TpipeErr) dup2(next[1], 2);
                close(next[1]);
                close(next[0]);
                /*Wow, a pat on my back for the above two lines*/
            }
            break;
        case Tapp:
        case TappErr:
            flags = O_RDWR | O_CREAT | O_APPEND;
            fileRedir(c, flags);
            break;
        case ToutErr:
        case Tout:
            flags = 0; 
            flags = O_RDWR | O_CREAT | O_TRUNC;
            fileRedir(c, flags);
            break;

        default:
            break;
    }

}

static void runCmd(Cmd c, int * prev, int * next, Job j)
{
    pid_t cpid, pid;
    int rc = 0;
    cpid = fork();

    if (cpid < 0) perror("fork error");
    if (cpid == 0) {

	pid = getpid();
	if (j) {
	    if (!j->pgid) j->pgid = pid;
	    setpgid(pid, j->pgid);
	}
    c->pid = pid;

	/*Child can receive all signals*/
    //if (is_job_fg(j))
	enable_signals();

    manageio(c, prev, next, j);

    if (is_cmd_builtin(c)) {
        rc = execute_builtin(c, j); 
        if (rc < 0) kill(SIGKILL, j->pgid);
        _exit(EXIT_SUCCESS);
    }
    else {
        if(execvp(c->args[0], c->args) < 0) 
        {
            kill(SIGKILL, j->pgid);
            handle_exec_error();
        }
    }

    } else { 
	c->pid = cpid;
	if (shell_interactive)
	{
	    /* j will be NULL in this case*/
	    /*To avoid race condition as described in man page, this has to be done in parent too*/
	    if(!j->pgid) j->pgid = cpid;
	    setpgid(cpid, j->pgid);
	}

    if (prev != NULL) 
    {
        close(prev[0]);
        close(prev[1]);
    }
    }
}

static void wait_all(Job j)
{
    wait_fg(j);
}

int is_cmd_builtin(Cmd c)
{
    int i;
    char *list[13] = {"end", "bg", "fg", "cd","pwd","echo","jobs","kill","logout","nice","where", "setenv", "unsetenv"};
    for (i = 0; i < 13; i++)
    {
        if (!strcmp(c->args[0], list[i])) 
        {
            return 1;
        }
    }
    return 0;
}

void prCmd(Cmd c, Job j, char *buf)
{
    int i;
    i = 0;
    builtin = is_cmd_builtin(c);
    
    if ( c ) {
	sprintf(buf + strlen(buf), "%s%s ", c->exec == Tamp ? "BG " : "", c->args[0]);
    if (j && is_job_fg(j)) {
        j->fg = 1; }
	
    if ( c->nargs > 1 ) {
	    sprintf(buf + strlen(buf),"[");
	    for ( i = 1; c->args[i] != NULL; i++ )
		sprintf(buf + strlen(buf),"%d:%s,", i, c->args[i]);
	    sprintf(buf + strlen(buf),"\b]");
	}
	
    if ( c->in == Tin )
	    sprintf(buf + strlen(buf), "<(%s) ", c->infile);
	if ( c->out != Tnil )
	    switch ( c->out ) {
		case Tout:
		    sprintf(buf + strlen(buf), ">(%s) ", c->outfile);
		    break;
		case Tapp:
		    sprintf(buf + strlen(buf),">>(%s) ", c->outfile);
		    break;
		case ToutErr:
		    sprintf(buf + strlen(buf),">&(%s) ", c->outfile);
		    break;
		case TappErr:
		    sprintf(buf + strlen(buf),">>&(%s) ", c->outfile);
		    break;
		case Tpipe:
		    sprintf(buf + strlen(buf),"| ");
		    break;
		case TpipeErr:
		    sprintf(buf + strlen(buf),"|& ");
		    break;
		default:
		    fprintf(stderr, "Shouldn't get here\n");
		    exit(-1);
	    }

	if (builtin && c->out != Tpipe && c->out != TpipeErr) {
        execute_builtin(c, j);
    } else {
        if (builtin) builtin = 0;
    }	

	//runCmd(c, prev, next);
    }
}

int execute_builtin(Cmd c, Job j)
{
    int rc = 0, fd = -1;
    int flags;
    if (c->in != Tnil) {
        printf("No builtin takes input");
        c->in = Tnil;
    }

    switch (c->out) {
        case Tnil:
            break;
        case Tapp:
            flags = O_RDWR | O_CREAT | O_APPEND;
            rc = fileRedir_builtin(c, flags, &fd);
        case Tout:
            flags = O_RDWR | O_CREAT | O_TRUNC;
            rc = fileRedir_builtin(c, flags, &fd);
            break;
        case TappErr:
            flags = O_RDWR | O_CREAT | O_APPEND;
            rc = fileRedir_builtin(c, flags, &fd);
            break;
        case ToutErr:
            flags = O_RDWR | O_CREAT | O_TRUNC;
            rc = fileRedir_builtin(c, flags, &fd);
            break;
        default:
            break;
    }
    if (rc != 0) fprintf(stderr, "%s: %s", strerror(errno), c->outfile);
    if (fd == -1) fd = 1; 
    if (!strcmp(c->args[0], "end") )
        exit(0);
    if (!strcmp(c->args[0], "bg"))
        background(find_job(c->args[1]));
    if (!strcmp(c->args[0], "fg" )) {
        foreground(find_job(c->args[1]));
    }
    if (!strcmp(c->args[0], "setenv" ))
        rc = set_env(c, &fd);
    if (!strcmp(c->args[0], "unsetenv" ))
        rc = unset_env(c);
    if (!strcmp(c->args[0], "jobs" ))
        list_jobs(&fd);
    if (!strcmp(c->args[0], "kill" ))
        killjob(find_job(c->args[1]));
    if (!strcmp(c->args[0], "where" ))
        whereis(c, &fd);
    if (!strcmp(c->args[0], "logout" ))
        log_out();
    if (!strcmp(c->args[0], "cd" ))
        rc = change_dir(c);
    if (!strcmp(c->args[0], "echo" ))
        echo_c(c, &fd);
    if (!strcmp(c->args[0], "pwd" ))
        pwd_c(c, &fd);
    if (!strcmp(c->args[0], "nice" ))
        niceness(c, j);
    if (j) j->first->head->completed = 1;
    if (rc < 0) fprintf(stderr, "%s\n", strerror(errno));
    return rc;
}

void echo_c(Cmd c, int * fd)
{
    char buf[100];
    memset(buf, 0, 100);
    int n = c->nargs - 1;
    while(n)
    {
        sprintf(buf+strlen(buf),"%s ",c->args[c->nargs - n]);
        n--;

    }
    dprintf(*fd, "%s\n", buf);
}

void pwd_c(Cmd c, int * fd)
{
    char * cur_dir = calloc(1, 128);
    getcwd(cur_dir, 128);
    dprintf(*fd, "%s\n", cur_dir);
    free(cur_dir);
}

int niceness(Cmd c, Job j)
{
    int rc = 0;
    pid_t pid;
    int num;

    if (strcmp(j->first->head->args[0], "nice")) {
        printf("Something is Wrong!");
    }

    if (c->nargs == 1)
    {
        rc = setpriority(PRIO_PROCESS, getpid(), 4);
        return rc;
    } 
    else {
        sscanf(c->args[1], "%d", &num);
        printf("%d\n", num);
        if (c->nargs == 2){
            rc = setpriority(PRIO_PROCESS, getpid(), num);
            return rc;

        }
        else {
            pid = fork();
            if (pid < 0) return -1;
            if (pid == 0) {
                rc = setpriority(PRIO_PROCESS, getpid(), num); 
                execvp(c->args[2], &(c->args[2]));
            }
            else {
                c->pid = pid;
                if (is_job_fg(j))
                    run_in_fg(j);
                else
                    run_in_bg(j);
            }
        }
    }
    return rc;
}

int change_dir(Cmd c)
{
    int rc;
    if (c->nargs == 1) rc = chdir(getenv("HOME"));
    else if(c->nargs == 2) rc = chdir(c->args[1]);
    return rc;
}

int set_env(Cmd c, int * fd)
{
    int rc;
    char **l = environ;
    if (c->nargs == 1)
    {
	int i;
	for (i = 0; l[i]; i++) 
    {
        if (builtin)
            dprintf(*fd, "%s\n", l[i]);
        else printf("%s\n", l[i]);
    }
    }
    else if (c->nargs == 2)
    {
	rc = setenv(c->args[1], "", 1);

    }
    else if (c->nargs == 3)
    {
	rc = setenv(c->args[1], c->args[2], 1);
    }
    return rc;

}

int unset_env(Cmd c)
{
    int rc;
    rc = unsetenv(c->args[1]);
    return rc;
}

void list_jobs(int * fd)
{
    Job j;
    if (first->next == NULL) {
        first->first->head->completed = 1;
        return;  /*There are no jobs since 'jobs' command is the only job*/
    }
    
    for (j = first; j; j = j->next)
    {
        if(is_job_stopped(j) && j->status != Killed)
            dprintf(*fd, "[%ld] \t Stopped \t\t %s\n", (long)j->number, j->command);
        else if(is_job_completed(j) && j->fg == 0)
            dprintf(*fd, "[%ld] \t Done \t\t %s\n", (long)j->number, j->command);
        else if(j->fg == 1 && j->pgid != 0 && j->status != Killed)
            dprintf(*fd, "[%ld] \t Running \t\t %s\n", (long)j->number, j->command);
        else if(j->fg == 0 && j->status != Killed) 
            dprintf(*fd, "[%ld] \t Running \t\t %s\n", (long)j->number, j->command);
        else if(j->status == Killed)
            dprintf(*fd, "[%ld] \t Terminated \t\t %s\n", (long)j->number, j->command);

    }
    return;

}

int foreground(Job j)
{
   if (j == NULL) return -1;
   tcsetpgrp(0, j->pgid);

   if (j->status != Running)
   {
       if (kill (-j->pgid, SIGCONT) < 0) {
           perror("SIGCONT");
           return -1;
       }
       j->status = Running;
   }
   wait_fg(j);
   j->status = Done;
   tcsetpgrp(0, par_pgid);

   return 0;
}

int background(Job j)
{
    if (j == NULL) return -1;
/* This can be called when a process is stopped. SIGCONT is sent*/
    if (kill(-j->pgid, SIGCONT) < 0) 
    {
	perror("SIGCONT");
	return -1;
    }
    j->status = Running;
    return 0;
}

int killjob (Job j)
{
    if (j == NULL) return -1;
    if (kill(-j->pgid, SIGKILL) < 0)
    {
        perror("SIGKILL");
        return -1;
    }
    j->status = Killed;
    return 0;
}

void log_out (Cmd c)
{
    _exit(EXIT_SUCCESS);
}

void whereis(Cmd c, int * fd)
{
    char *list[12] = {"bg", "fg", "cd","pwd","echo","jobs","kill","logout","nice","where", "setenv", "unsetenv"};
int i;
pid_t pid;
int status;

for (i = 0; i < 12; i++)
{
    if(!strcmp(c->args[1], list[i])) {
        dprintf(*fd, "%s: shell built-in command\n", list[i]);
    }
}

pid = fork();
if (pid < 0) return;
if(pid == 0) {
    execvp("/usr/bin/which", c->args);
    c->pid = getpid();
}
else 
{
    c->pid = pid;
    waitpid(pid, &status, WUNTRACED);
    if (pid > 0) change_proc_status(pid, status);

}
}

void sig_handler(int signo)
{
    printf("In sig handler\n");
}
void enable_signals(void)
{
    signal (SIGINT, SIG_DFL);
    signal (SIGQUIT, SIG_DFL);
    signal (SIGTSTP, SIG_DFL);
    signal (SIGTTIN, SIG_DFL);
    signal (SIGTTOU, SIG_DFL);
    signal (SIGHUP, SIG_DFL);
}
void disable_signals(void) 
{

    //signal (SIGINT, SIG_IGN);
    signal (SIGQUIT, SIG_IGN);
    signal (SIGTSTP, SIG_IGN);
    signal (SIGTTIN, SIG_IGN);
    signal (SIGTTOU, SIG_IGN);
    signal (SIGHUP, SIG_IGN);
}

static void prPipe(Pipe p, Job j)
{
    int i = 0, status;
    int pipefd[10][2];
    int *x = NULL, *y = NULL;
    Cmd c = NULL;

    if ( p == NULL )
	return;

    //printf("Begin pipe%s\n", p->type == Pout ? "" : " Error");
    for ( c = p->head; c != NULL; c = c->next ) {
	//printf("  Cmd #%d: ", ++i);
	++i;
	if (c->out == Tpipe || c->out == TpipeErr) {
	    pipe(pipefd[i]);
	    if (i == 0) x = NULL;
	    y = pipefd[i];
	} 
	if (c->in == Tpipe || c->in == TpipeErr) {
	    x = pipefd[i-1];
	    y = pipefd[i];
	}
	prCmd(c,j,command_str);
    if (!builtin)
	    runCmd(c, x, y, j);
    }
    //printf("End pipe\n");
    if (!j) {
        while(waitpid(-1, &status, WUNTRACED) != -1);
        prPipe(p->next, NULL);
    } 
}

static void prJob(Job j)
{
    if(j == NULL) return;
    memset(command_str, 0, strlen(command_str));
    //for (p = j->first; p; p = p->next) prPipe(p, j);
    prPipe(j->first , j);
    memcpy(j->command, command_str, strlen(command_str) + 1);
    //j->command[strlen(command_str)] = '\0';	
    
    if(is_job_stopped(j) || is_job_completed(j)) 
        return;

    /*This must be done when ush reads ~/.ushrc*/
    if (!shell_interactive) wait_all(j);

    /*Interactive shell*/
    else if (j->fg) run_in_fg(j);
    else run_in_bg(j);

}

int main(int argc, char *argv[])
{
    Job j = NULL;
    Pipe p = NULL;
    shell_term = STDIN_FILENO;
    char *host = getenv("USER");
    shell_interactive = 0;
    command_str = calloc(1, 512);
    int stdin_copy = dup(0);
    close(0);

    /* Initialize shell and read from ~/.ushrc*/
    int rc_fd = -1;
    char * home = getenv("HOME");
    char *ushrc = NULL;
    if (home) {
        ushrc = calloc(1, strlen(home) + 9);
        snprintf(ushrc, strlen(home) + 8, "%s/.ushrc", home);
    }
    else {
        ushrc = calloc(1, 9);
        snprintf(ushrc, 8, "./.ushrc");
    }
    rc_fd = open(ushrc, O_RDONLY);
    if (rc_fd < 0) 
    {
        shell_interactive = 1;
        fprintf(stderr, "%s: %s\n", strerror(errno), ushrc); 
    } else {
        FILE * fp = fdopen(rc_fd, "r");
        while (!feof(fp))
        {
            p = parse();
            if (!strcmp(p->head->args[0],"end")) {
                freePipe(p);
                break;
            }
            prPipe(p, NULL);
            freePipe(p);
        }
        close(rc_fd);
        fclose(fp);
        fflush(stdout);
        free(ushrc);
    } 
    dup2(stdin_copy, 0);
	close(stdin_copy);
	shell_interactive = 1;
    disable_signals();

	/*Put the shell in its own process group*/
	par_pgid = getpid();
	if (setpgid(par_pgid, par_pgid) < 0) handle_error("setpgid");

    tcsetpgrp (shell_term, par_pgid);
    
    while (shell_interactive) 
    {
        wait_bg();
        printf("%s%% ", host);
        fflush(stdout);
        p = parse();
        while(p) {
            j = create_job(p);
            prJob(j);
            p = p->next;
        }
        freePipe(p);
    }
}
/*........................ end of main.c ....................................*/
