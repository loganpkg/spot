#
# Copyright (c) 2023, 2024 Logan Ryan McLintock. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

CFLAGS = -ansi -g -Og -Wno-variadic-macros -Wall -Wextra -pedantic -I .
apps = spot m4 bc freq tornado_dodge

.PHONY: all

all: ${apps}

curses.o: curses.h

gen.o: toucanlib.h
num.o: toucanlib.h
buf.o: toucanlib.h
gb.o: toucanlib.h
eval.o: toucanlib.h
ht.o: toucanlib.h
fs.o: toucanlib.h
toco_regex.o: toucanlib.h

toucanlib.o: gen.o num.o buf.o gb.o eval.o ht.o toco_regex.o fs.o
	ld -r gen.o num.o buf.o gb.o eval.o ht.o toco_regex.o fs.o \
		-o toucanlib.o

spot: spot.c curses.o toucanlib.o
m4: m4.c toucanlib.o
bc: bc.c toucanlib.o
freq: freq.c toucanlib.o
tornado_dodge: tornado_dodge.c curses.o toucanlib.o

.PHONY: install
install:
	mkdir -p ${PREFIX}/bin
	cp -a ${apps} ${PREFIX}/bin

.PHONY: clean
clean:
	rm -f *.o *.obj *.lib *.ilk *.pdb *.exe ${apps}
