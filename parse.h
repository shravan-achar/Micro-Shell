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

void freePipe(Pipe);
Pipe parse();
void fileRedir(Cmd c, int flags);
#endif /* PARSE_H */
/*........................ end of parse.h ...................................*/
