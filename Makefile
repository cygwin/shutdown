# Makefile for shutdown
#
# Copyright (C) 2001, 2012, Corinna Vinschen
# 
# This file is part of shutdown for Cygwin.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

SRCDIR ?= .
VPATH = $(SRCDIR)

BINDIR=/usr/bin
DESTDIR=

CFLAGS=-g
LDFLAGS=-s

EXEEXT=.exe

OBJ=shutdown.o

all:	shutdown$(EXEEXT)

shutdown$(EXEEXT): $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ)

install: shutdown$(EXEEXT)
	mkdir -p $(DESTDIR)$(BINDIR)
	cp shutdown$(EXEEXT) $(DESTDIR)$(BINDIR)
	cd $(DESTDIR)$(BINDIR) && \
	ln -fs shutdown$(EXEEXT) reboot && \
	ln -fs shutdown$(EXEEXT) hibernate && \
	ln -fs shutdown$(EXEEXT) suspend

clean:
	rm -f shutdown$(EXEEXT) shutdown.o *.stackdump

distclean: clean
