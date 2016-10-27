# Micro-Shell
ush is a command interpreter with a syntax similar to UNIX C shell, csh(1).
This lightweight shell supports IO redirections, pipes and provides implementations for cd, pwd, echo, nice, setenv, unsetenv, where(like which in bash) etc.
Also provides job control. Commands fg %n, bg %n, kill %n, jobs are supported. Signal handling is supported. 
Reads $HOME/.ushrc on startup. 
