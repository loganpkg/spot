#
# Copyright (c) 2023, 2024 Logan Ryan McLintock. All rights reserved.
#
# The contents of this file are subject to the
# Common Development and Distribution License (CDDL) version 1.1
# (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License in
# the LICENSE file included with this software or at
# https://opensource.org/license/cddl-1-1
#
# NOTICE PURSUANT TO SECTION 4.2 OF THE LICENSE:
# This software is prohibited from being distributed or otherwise made
# available under any subsequent version of the License.
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
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
