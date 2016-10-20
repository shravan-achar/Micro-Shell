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
