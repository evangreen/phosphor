################################################################################
#
#   Copyright (c) 2010 Evan Green
#
#   Module Name:
#
#       Phosphor2 Screen Saver
#
#   Abstract:
#
#       This file specifies how to build the phosphor2 screen saver.
#
#   Author:
#
#       Evan Green 24-Feb-2010
#
#   Environment:
#
#       Windows
#
################################################################################

BINARY = phosphor2.scr

OBJS = phosphor2.o config.rsc

CC = gcc
LD = ld
RCC = windres

CCOPTIONS = -Wall -Werror -c -I include -I .

.PHONY: all clean

all: $(BINARY)

$(BINARY): $(OBJS)
	@echo Linking $@
	@$(CC) -mwindows -o $@ $^ -lwinmm

%.o:%.c
	@echo Compiling - $<
	@$(CC) $(CCOPTIONS) -o $@ $<

%.rsc:%.rc
	@echo Compiling Resource - $<
	@$(RCC) -o $@ $<

clean:
	-del *.o
	-del *.rsc
	-del $(BINARY)
