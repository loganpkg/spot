#! /bin/sh

#
# Copyright (c) 2023 Logan Ryan McLintock
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

set -e
set -u
set -x

#################
# Configuration #
#################
install_dir="$HOME"/bin
flags='-ansi -g -Og -Wno-variadic-macros -Wall -Wextra -pedantic'
# Change to 'N' to use ncurses
use_built_in_curses='Y'
#################


if [ "$(uname)" = Linux ]
then
    indent=indent
else
    indent=gindent
fi

repo_dir="$(pwd)"
build_dir="$(mktemp -d)"

# Fix permissions
find . -type d ! -path '*.git*' -exec chmod 700 '{}' \;
find . -type f ! -path '*.git*' ! -name '*.sh' -exec chmod 600 '{}' \;
find . -type f ! -path '*.git*' -name '*.sh' -exec chmod 700 '{}' \;

# Copy files
find . -type f ! -path '*.git*' -exec cp -p '{}' "$build_dir" \;

cd "$build_dir" || exit 1

rm -f err

find . -type f \( -name '*.h' -o -name '*.c' \) \
    -exec "$indent" -nut -kr -T WINDOW -T bool -T M4ptr -T Fptr '{}' \; 2> err

if [ -s err ]
then
    cat err
    exit 1
fi

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
    ! -name 'spot.c' ! -name 'tornado_dodge.c' \
    -exec cc -c $flags '{}' \;

if [ "$use_built_in_curses" = 'Y' ]
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

if [ "$use_built_in_curses" = 'Y' ]
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
