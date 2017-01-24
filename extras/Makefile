#
# Makefile for releasing shutdown
#
# Copyright (C) 2016, Frank Fesevur
# 
# This file is part of shutdown for Cygwin.
#

all: x32 x64

x32:
	cygport --32 shutdown.cygport download prep compile install package

x64:
	cygport --64 shutdown.cygport download prep compile install package

upload:
	cygport --32 shutdown.cygport upload
	cygport --64 shutdown.cygport upload

clean:
	cygport --32 shutdown.cygport clean
	cygport --64 shutdown.cygport clean
	rm shutdown-*.tar.bz2
