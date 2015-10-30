# $Id: Makefile,v 1.3 2010/09/19 16:37:28 jm Exp $
# Copyright (c) 2004, 2009  Jean-Marc Bourguet
# 
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
# 
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the
#       distribution.
# 
#     * Neither the name of Jean-Marc Bourguet nor the names of the other
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
# IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

SUBDIRS=src doc
prefix=/opt/bourguet/tapetools

all clean realclean distclean:
	@for dir in $(SUBDIRS); do \
	   (cd $$dir; $(MAKE) $@); \
	done;  	      	      	   \

clean: clean.pre

clean.pre:
	@-rm *~

realclean: clean
distclean: clean

dist:
	$(MAKE) realclean
	$(MAKE) all
	$(MAKE) distclean
	cd .. ; tar cjfX tapetools.tar.bz tapetools/excluded tapetools

install: all $(DESTDIR)$(prefix)/bin $(DESTDIR)$(prefix)/man/man1
	cp src/extractTape src/buildTape $(DESTDIR)$(prefix)/bin
	cp doc/extractTape.1 doc/extractTape.1 $(DESTDIR)$(prefix)/man/man1

$(DESTDIR)$(prefix)/bin:
	@mkdir -p $(DESTDIR)$(prefix)/bin

$(DESTDIR)$(prefix)/man/man1:
	@mkdir -p $(DESTDIR)$(prefix)/man/man1
