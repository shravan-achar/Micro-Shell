#include "parse.h"

void fileRedir(Cmd c, int flags)
{
	int fdi, fdo;
	int in = c->in;
	int out = c->out;
	if (in == Tin) {     
		fdi = openat(AT_FDCWD, c->infile, flags, 0666);
		if (fdi < 0 ) handle_error("openat");
		dup2(fdi, 0);
		close(fdi);

	}
	else if (out == ToutErr || out == Tout || out == TappErr || out == Tapp) {
		fdo = openat(AT_FDCWD, c->outfile, flags, 0666);
		if (fdo < 0 ) handle_error("openat");

		dup2(fdo, 1);
		if (c->out == ToutErr || c->out == TappErr) dup2(fdo, 2);
		close(fdo);
	}
}


/* Job control utilities*/

int is_job_stopped (Job j)
{
    Pipe p;
    Cmd c;

    for (p=j->first; p; p = p->next)
    {
	for(c = p->head; c; c = c->next)
	{
	    /*Only if all processes are either stopped or completed*/
	    if (!(c->completed) && !(c->stopped)) return 0;
	}
    }
    return 1;
}

int is_job_completed (Job j)
{
/* Same routine as last one except for the if condition*/    
    Pipe p;
    Cmd c;
    
    for (p=j->first; p; p = p->next)
    {
	for(c = p->head; c; c = c->next)
	{
	    /*Only if all processes are completed
	     No separate status for termination yet*/
	    if (!(c->completed)) return 0;
	}
    }
    return 1;

}

int get_job_number()
{
    /* A new job can not have a lower number than an existing job in the job list*/
    Job j = first;
    if (first == NULL) return 1;
    while(j->next) j = j->next;
    return (j->number + 1);
}

int is_job_fg(Job j)
{
    /*This logic is due to a bug in the parser. If any of the intermediate commands are BG then entire job is BG*/
    /*This logic returns 0 only if the last command in a job is followed by a &*/
    Pipe p;
    Cmd c;
    p = j->first;
    while(p->next) p = p->next;
    c = p->head;
    while(c->next) c = c->next;
    if (c->exec == Tamp) return 0;
    return 1;

}

void run_in_fg(Job j)
{
/*Put the job in fg*/
tcsetpgrp(0, j->pgid);

/*Wait for it to complete*/
wait_fg(j);

/*Put shell back in fg*/
tcsetpgrp(0, par_pgid);
}

void run_in_bg(Job j)
{
/*Do nothing since shell is already in fg and not waiting*/
	    fprintf(stdout, "[%d] %ld\n", j->number, (long)j->pgid);
    return;
}

int change_proc_status(pid_t pid, int status)
{
    Job j;
    Pipe p;
    Cmd c;
    if (pid == -1)
    {
	/*pid can be -1 for other errors*/	
	/*All children have exited*/
	//if (errno == ECHILD) printf("%s\n",strerror(errno));
	return -1;
    }
    if(pid == 0)
    {
	/*waitpid was called with WNOHANG. Done for bg jobs. No status change in child*/
	return -1;
	/*pid can not be zero if WNOHANG is not used, hence it is safe to return -1*/
    }
    
    for (j=first; j; j = j->next) 
    {
	for (p = j->first; p; p = p->next) 
	{
	    for (c = p->head; c; c = c->next)
	    {
		if (c->pid == pid)
		{
		    if(WIFSTOPPED(status)) c->stopped = 1;
		    else c->completed = 1; /*Anything else is complete, including termination*/
		    return 0;
		}

	    }
	}
    }
    return -1;
}

void wait_bg()
{
    pid_t pid;
    int status;
    
    do {
	pid = waitpid(-1, &status, WUNTRACED | WNOHANG);
	/* Since any fg or bg process may have completed or stopped, all the jobs need to be traversed*/
    }while(!change_proc_status(pid, status));

    if (pid) clean_finished_jobs();
}

void wait_fg(Job j)
{
    pid_t pid;
    int status;

    do {
	pid = waitpid(-1, &status, WUNTRACED);
	/* Since any fg or bg process may have completed or stopped, all the jobs need to be traversed*/
	if (change_proc_status(pid, status) < 0) 
	{
	    clean_finished_jobs();
	    break;

	}
    }while(!(is_job_stopped(j)) && !(is_job_completed(j)));
}

Job create_job(Pipe p)
{
    if (p == NULL) return NULL;
    Job j = first;
    int number = get_job_number();
    int i = 1;
    while(i < number)
    {
	j = j->next;
	i++;

    }
    assert(!j);// j should be null 
    j = malloc(sizeof(*j));
    memset(j, 0, sizeof(*j));
    if(j  == NULL) handle_error("Job malloc");
    j->number = number;
    j->user_updated = 0;
    j->pgid = 0; /* Updated later in the code*/
    j->first = p;
    j->next = NULL;
    if (number == 1) first = j;
    return j;
}

void clean_finished_jobs()
{
    Job j,last = NULL;

    for (j = first; j; j = j->next)
    {
	if (is_job_completed(j))
	{
	    fprintf(stdout, "[%d] \t Done \t\t %s\n", j->number, j->command);
	    if (last) last->next = j->next;
	    else first = j->next;
	    free(j);

	}
	else if (is_job_stopped(j)) 
	{
	    if (!j->user_updated) 
	    {
		fprintf(stdout, "[%d] \t stopped \t\t %s\n", j->pgid, j->command);
		j->user_updated = 1;
	    }
	    last = j;

	}
	else last = j;/*Need this to delete*/
    }
}

