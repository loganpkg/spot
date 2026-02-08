#! /bin/sh

#
# Copyright (c) 2023-2025 Logan Ryan McLintock. All rights reserved.
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

set -e
set -u
set -x

#################
# Configuration #
#################
install_dir="$HOME"/bin
flags='-ansi -g -Og -Wno-variadic-macros -Wall -Wextra -pedantic'
# Change to N to use ncurses
use_built_in_curses=Y
#################


if [ "$(uname)" = Linux ]
then
    indent=indent
else
    indent=gindent
fi

# lint=splint

repo_dir=$(pwd)
build_dir=$(mktemp -d)

# Fix permissions
find . -type d ! -path '*.git*' -exec chmod 700 '{}' \;
find . -type f ! -path '*.git*' ! -name '*.sh' -exec chmod 600 '{}' \;
find . -type f ! -path '*.git*' -name '*.sh' -exec chmod 700 '{}' \;

# Copy files
find . -type f ! -path '*.git*' -exec cp -p '{}' "$build_dir" \;

cd "$build_dir" || exit 1

rm -f err

find . -type f \( -name '*.h' -o -name '*.c' \) \
    -exec "$indent" -nut -kr \
        -T size_t \
        -T FILE \
        -T WINDOW \
        -T bool \
        -T M4ptr \
        -T Fptr \
        '{}' \; 2> err

if [ -s err ]
then
    cat err
    exit 1
fi

# find . -type f \( -name '*.h' -o -name '*.c' \) \
#    -exec "$lint" -predboolint '{}' \;


# Max of 79 chars per line plus the \n, making a total of 80.
# So 80 non-newline chars is too long.
find . -type f ! -path '*.git*' ! -name '*~' \
    -exec grep -H -n -E '.{80}' '{}' \;


./func_dec.sh toucanlib.h gen.c num.c buf.c gb.c eval.c ht.c \
    toco_regex.c fs.c

./func_dec.sh curses.h curses.c

find . -type f ! -path '*.git*' -name '*.h' \
    -exec cc $flags '{}' \;

find . -type f ! -path '*.git*' -name '*.c' \
    ! -name spot.c ! -name tornado_dodge.c \
    -exec cc -c $flags '{}' \;

if [ "$use_built_in_curses" = Y ]
then
    # To look in the current working directory for <curses.h>
    cc -c $flags -I . spot.c
    cc -c $flags -I . tornado_dodge.c
else
    cc -c $flags spot.c
    cc -c $flags tornado_dodge.c
fi

ld -r gen.o num.o buf.o gb.o eval.o ht.o toco_regex.o fs.o -o toucanlib.o

cc $flags -o m4 m4.o toucanlib.o

if [ "$use_built_in_curses" = Y ]
then
    cc $flags -o spot spot.o curses.o toucanlib.o
    cc $flags -o tornado_dodge tornado_dodge.o curses.o toucanlib.o
else
    cc $flags -o spot spot.o toucanlib.o -lncurses
    cc $flags -o tornado_dodge tornado_dodge.o toucanlib.o -lncurses
fi


cc $flags -o bc bc.o toucanlib.o
cc $flags -o freq freq.o toucanlib.o


mkdir -p "$install_dir"

cp -p m4 spot bc freq tornado_dodge "$install_dir"/

m4 test.m4 > .k
/usr/bin/m4 test.m4 > .k2
cmp .k .k2

# Update files
find . -type f \( -name '*.h' -o -name '*.c' \) -exec cp -p '{}' "$repo_dir" \;
