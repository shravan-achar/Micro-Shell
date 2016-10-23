# Makefile for ush
#
# Vincent W. Freeh
# 

CC=gcc
CFLAGS=-g -Wall
SRC=main.c parse.c utilities.c
OBJ=main.o parse.o utilities.o

ush:	$(OBJ)
	$(CC) -o $@ $(OBJ)

tar:
	tar czvf ush.tar.gz $(SRC) Makefile README

clean:
	\rm $(OBJ) ush

ush.1.ps:	ush.1
	groff -man -T ps ush.1 > ush.1.ps
