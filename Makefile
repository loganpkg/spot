#
# Copyright (c) 2024 Logan Ryan McLintock
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
