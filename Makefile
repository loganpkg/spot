#
# Copyright (c) 2024 Logan Ryan McLintock
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
#

CFLAGS = -ansi -g -Og -Wall -Wextra -pedantic -I .
apps = spot m4 bc freq lsed

.PHONY: all

all: ${apps}

curses.o: curses.h

gen.o: toucanlib.h
num.o: toucanlib.h
expanded_buf.o: toucanlib.h
gb.o: toucanlib.h
eval.o: toucanlib.h
ht.o: toucanlib.h
regex.o: toucanlib.h
fs.o: toucanlib.h

toucanlib.o: gen.o num.o expanded_buf.o gb.o eval.o ht.o regex.o fs.o
	ld -r gen.o num.o expanded_buf.o gb.o eval.o ht.o regex.o fs.o \
		-o toucanlib.o

spot: spot.c curses.o toucanlib.o
m4: m4.c toucanlib.o
bc: bc.c toucanlib.o
freq: freq.c toucanlib.o
lsed: lsed.c toucanlib.o

.PHONY: install
install:
	mkdir -p ${PREFIX}/bin
	cp -a ${apps} ${PREFIX}/bin

.PHONY: clean
clean:
	rm -f *.o ${apps}
