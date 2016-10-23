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
#include "parse.h"

static void handle_exec_error() 
{
    switch(errno) {

	case EPERM: 
	    printf("permission denied\n");
	    break;
	case ENOENT:
	    printf("command not found\n");
	    break;
    }

    _exit(EXIT_FAILURE);
}

static void runCmd(Cmd c, int * prev, int * next, Job j)
{
    pid_t cpid, pid;
    int flags;
    cpid = fork();

    if (cpid < 0) perror("fork error");
    if (cpid == 0) {

	pid = getpid();
	if (j) {
	    if (!j->pgid) j->pgid = pid;
	    setpgid(pid, j->pgid);
	}

	/*Child can receive all signals*/
	enable_signals();
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
	if (execvp(c->args[0], c->args) < 0) handle_exec_error();

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


void prCmd(Cmd c, Job j, char *buf)
{
    int i;

    if ( c ) {
	sprintf(buf + strlen(buf), "%s%s ", c->exec == Tamp ? "BG " : "", c->args[0]);
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

	if ( c->nargs > 1 ) {
	    sprintf(buf + strlen(buf),"[");
	    for ( i = 1; c->args[i] != NULL; i++ )
		sprintf(buf + strlen(buf),"%d:%s,", i, c->args[i]);
	    sprintf(buf + strlen(buf),"\b]");
	}
	
	if (c->out == Tnil) {
	    if (!strcmp(c->args[0], "end") )
		exit(0);
	    if (!strcmp(c->args[0], "bg"))
		background(j);
	    if (!strcmp(c->args[0], "fg" ))
		foreground(j);
	    if (!strcmp(c->args[0], "setenv" ))
		set_env(c);
	    if (!strcmp(c->args[0], "unsetenv" ))
		unset_env(c);
	    if (!strcmp(c->args[0], "jobs" ))
		list_jobs();
	    if (!strcmp(c->args[0], "kill" ))
		killjob(j);
	    if (!strcmp(c->args[0], "where" ))
		whereis(c);
	    if (!strcmp(c->args[0], "logout" ))
		log_out();
	    if (!strcmp(c->args[0], "cd" ))
		change_dir(c);
	    //if (!strcmp(c->args[0], "echo" ))
	//	echo_c(c);
	 //   if (!strcmp(c->args[0], "pwd" ))
	//	pwd_c(c);
	}	
	//TODO builtins

	//runCmd(c, prev, next);
    }
}
void echo_c(Cmd c)
{
    execvp("/bin/echo", c->args);
}

void pwd_c(Cmd c)
{
    execvp("/bin/pwd",c->args);
}

int change_dir(Cmd c)
{
    int rc;
    if (c->nargs == 1) rc = chdir(getenv("HOME"));
    else if(c->nargs == 2) rc = chdir(c->args[1]);
    return rc;
}

int set_env(Cmd c)
{
    int rc;
    char **l = environ;
    if (c->nargs == 1)
    {
	int i;
	for (i = 0; l[i]; i++) printf("%s\n", l[i]);
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

void list_jobs()
{
    Job j;
    for (j = first; j; j = j->next)
    {

	    fprintf(stdout, "[%ld] \t %d \t\t %s\n", (long)j->number, j->status, j->command);

    }

}

int foreground(Job j)
{
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

void whereis(Cmd c)
{
    char *list[12] = {"bg", "fg", "cd","pwd","echo","jobs","kill","logout","nice","where", "setenv", "unsetenv"};
int i;

for (i = 0; i < 10; i++)
{
    if(!strcmp(c->args[1], list[i])) {
	printf("%s: shell built-in command;", list[i]);
    }
}

execvp("/usr/bin/which", c->args);

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
    signal (SIGCHLD, SIG_DFL);
}
void disable_signals(void) 
{

//    signal (SIGINT, SIG_IGN);
//    signal (SIGQUIT, SIG_IGN);
    signal (SIGTSTP, SIG_IGN);
    signal (SIGTTIN, SIG_IGN);
    signal (SIGTTOU, SIG_IGN);
//    signal (SIGCHLD, SIG_IGN);
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
    Pipe p;
    if(j == NULL) return;
    memset(command_str, 0, strlen(command_str));
    for (p = j->first; p; p = p->next) prPipe(p, j);
    j->command = command_str;
	
    /*This must be done when ush reads ~/.ushrc*/
    if (!shell_interactive) wait_all(j);

    /*Interactive shell*/
    else if (is_job_fg(j)) run_in_fg(j);
    else run_in_bg(j);

}

int main(int argc, char *argv[])
{
    Job j = NULL;
    Pipe p = NULL;
    char *host = getenv("USER");
    shell_interactive = 0;
    command_str = calloc(1, 512);
    int stdin_copy = dup(0);
    close(0);

    /* Initialize shell and read from ~/.ushrc*/
    int rc_fd = -1;
    rc_fd = open("/home/shravan/.ushrc", O_RDONLY);
    FILE * fp = fdopen(rc_fd, "r");
    if (rc_fd < 0) 
    {
	shell_interactive = 1;
	perror("Open ~/.ushrc"); 
    } else {
	
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
    } 
	dup2(stdin_copy, 0);
	close(stdin_copy);
	shell_interactive = 1;
        disable_signals();

	/*Put the shell in its own process group*/
	par_pgid = getpid();
	if (setpgid(par_pgid, par_pgid) < 0) handle_error("setpgid");
	
   while (shell_interactive) 
   {
       wait_bg();
       printf("%s%% ", host);
       fflush(stdout);
       p = parse();
       j = create_job(p);
       prJob(j);
       freePipe(p);
   }
}


/*........................ end of main.c ....................................*/
