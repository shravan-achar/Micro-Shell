/******************************************************************************
 *
 *  File Name........: parse.h
 *
 *  Description......: header file for the ash shell parser.
 *
 *  Author...........: Vincent W. Freeh
 *
 *****************************************************************************/

#ifndef PARSE_H
#define PARSE_H

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>


#define _GNU_SOURCE

#define handle_error(msg) \
     do { perror(msg); _exit(-1); } while(0)

/* list of all tokens */
typedef enum {Terror, Tword, Tamp, Tpipe, Tsemi, Tin, Tout,
	      Tapp, TpipeErr, ToutErr, TappErr, Tnl, Tnil, Tend} Token;

/* cmd data structure
 * linked list, one cmd_T for each cmd in a pipe 
 */
struct cmd_t {
  Token exec;			/* whether background or foreground */
  Token in, out;		/* determines where input/output comes/goes*/
  char *infile, *outfile;	/* set if file redirection */
  int nargs, maxargs;		/* num args in args array below (and size) */
  char **args;			/* argv array -- suitable for execv(1) */
  pid_t pid;
  char completed;         /*Could be completed or terminated*/      
  char stopped;                  /*Could be blocked or suspended*/
  struct cmd_t *next;
};
typedef struct cmd_t *Cmd;

/* pipe type -- either: | or |& */
typedef enum {Pout, PoutErr} Ptype;

/* pipe data structure
 * linked list, one pipe_t for each pipe on a line.
 */
struct pipe_t {
  Ptype type;
  Cmd head;
  struct pipe_t *next;
};

typedef struct pipe_t *Pipe;

typedef enum {Running, Stopped, Done, Terminated, Killed} Status;
struct jobs_t {
    int number; /* Job number*/ 
    pid_t pgid; /*Process group ID*/
    Pipe first; /*First pipe*/
    struct jobs_t *next; /*Next job*/
    char user_updated; /*Status is updated to the user*/    
    char * command;
    int fg; /*Is this job background or foreground*/
    Status status;
};

typedef struct jobs_t * Job;
Job first;
pid_t par_pgid; /*Same as parent(ushell) pid*/
int shell_interactive;
int shell_term;
char * command_str;
int builtin;
extern char **environ;

void freePipe(Pipe);
Pipe parse();
void prCmd(Cmd c, Job j, char *buf);
void enable_signals();
void disable_signals();
void fileRedir(Cmd c, int flags);
int is_job_completed(Job j);
int is_job_stopped(Job j);
int get_job_number();
int is_job_fg(Job j);
void run_in_fg(Job j);
void run_in_bg(Job j);
int change_proc_status(pid_t pid, int status);
char * getcommand(Job j);
void clean_finished_jobs();
void wait_fg(Job j);
void wait_bg();
Job create_job(Pipe p);
int background(Job j);
int foreground(Job j);
void echo_c(Cmd c);
void whereis(Cmd c);
void pwd_c(Cmd c);
int set_env(Cmd c);
int unset_env(Cmd c);
int killjob(Job j);
void list_jobs();
void log_out();
int change_dir(Cmd c);
Job find_job(char * num);
int is_cmd_builtin(Cmd c);
void execute_builtin(Cmd c, Job j);
int niceness(Cmd c);

#endif /* PARSE_H */
/*........................ end of parse.h ...................................*/
