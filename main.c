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

static void runCmd(Cmd c, int * prev, int * next)
{
    pid_t cpid, pid;
    int fd = -1, flags;
    cpid = fork();

    if (cpid < 0) perror("fork error");
    if (cpid == 0) {

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
	if (prev != NULL) 
	{
	    close(prev[0]);
	    close(prev[1]);
	}
	return;
    }
}


static void prCmd(Cmd c, int *prev, int *next)
{
    int i;

    if ( c ) {
	printf("%s%s ", c->exec == Tamp ? "BG " : "", c->args[0]);
	if ( c->in == Tin )
	    printf("<(%s) ", c->infile);
	if ( c->out != Tnil )
	    switch ( c->out ) {
		case Tout:
		    printf(">(%s) ", c->outfile);
		    break;
		case Tapp:
		    printf(">>(%s) ", c->outfile);
		    break;
		case ToutErr:
		    printf(">&(%s) ", c->outfile);
		    break;
		case TappErr:
		    printf(">>&(%s) ", c->outfile);
		    break;
		case Tpipe:
		    printf("| ");
		    break;
		case TpipeErr:
		    printf("|& ");
		    break;
		default:
		    fprintf(stderr, "Shouldn't get here\n");
		    exit(-1);
	    }

	if ( c->nargs > 1 ) {
	    printf("[");
	    for ( i = 1; c->args[i] != NULL; i++ )
		printf("%d:%s,", i, c->args[i]);
	    printf("\b]");
	}
	putchar('\n');
	// this driver understands one command
	if ( !strcmp(c->args[0], "end") )
	    exit(0);

	runCmd(c, prev, next);
    }
}
void sig_handler(int signo)
{
    printf("In sig handler\n");
}

static void signal_routine(void) 
{

    signal(SIGQUIT, SIG_IGN);
    //signal(SIGINT, SIG_IGN);

    /* Handle SIGTERM and SIGTSTP */

    /*Send this to the running foreground process*/
    signal(SIGTSTP, sig_handler);
}

static void prPipe(Pipe p)
{
    int i = 0, status;
    int pipefd[10][2];
    int * x, *y;
    Cmd c;

    if ( p == NULL )
	return;

    printf("Begin pipe%s\n", p->type == Pout ? "" : " Error");
    for ( c = p->head; c != NULL; c = c->next ) {
	printf("  Cmd #%d: ", ++i);
	if (c->out == Tpipe || c->out == TpipeErr) {
	    pipe(pipefd[i]);
	    if (i == 0) x = NULL;
	} 
	if (c->in == Tpipe || c->in == TpipeErr) {
	    x = pipefd[i-1];
	}
	y = pipefd[i];
	prCmd(c, x, y);
    }
    printf("End pipe\n");
    while(waitpid(-1, &status, 0) != -1);
    prPipe(p->next);
}

int main(int argc, char *argv[])
{
    Pipe p;
    char c;
    char *host = getenv("USER");
    int shell_interactive = 0;
    int stdin_copy = dup(0);
    close(0);
    signal_routine();

    /* Initialize shell and read from ~/.ushrc*/
    int rc_fd = -1;
    rc_fd = open("/home/shravan/.ushrc", O_RDONLY);
    printf("rc_fd %d\n", rc_fd);
    FILE * fp = fdopen(rc_fd, "r");
    if (rc_fd < 0) 
    {
	shell_interactive = 1;
	perror("Open ~/.ushrc"); 
    } else {
	long eof = lseek(rc_fd, 0, SEEK_END);
	long offt;
	offt = lseek(rc_fd, 0, SEEK_SET);
	//dup2(rc_fd, 0);
    while (!feof(fp))
	{
	    p = parse();
	    if (!strcmp(p->head->args[0],"end")) {
		freePipe(p);
		break;
	    }
	    prPipe(p);
	    freePipe(p);
	}
	close(rc_fd);
	fclose(fp);
    } 
	dup2(stdin_copy, 0);
	close(stdin_copy);
	shell_interactive = 1;
    
    while (shell_interactive) {
	printf("%s%% ", host);
	fflush(stdout);
	p = parse();
	prPipe(p);
	freePipe(p);
    }
}

/*........................ end of main.c ....................................*/
