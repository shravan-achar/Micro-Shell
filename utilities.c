#include "parse.h"

void fileRedir(Cmd c, int flags)
{
    int fd;
    fd = openat(AT_FDCWD, c->outfile, flags, 0666);
    if (fd < 0) handle_error("openat");
    if (c->out == ToutErr || c->out == TappErr) dup2(fd, 2);
    dup2(fd, 1);
    close(fd);
}
