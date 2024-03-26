#! /bin/sh

#
# Copyright (c) 2023 Logan Ryan McLintock
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

set -e
set -u
set -x

# Configuration
install_dir="$HOME"/bin
flags='-ansi -g -Og -Wall -Wextra -pedantic'

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
    -exec "$indent" -nut -kr '{}' \; 2> err

if [ -s err ]
then
    cat err
    exit 1
fi

# Max of 79 chars per line plus the \n, making a total of 80.
# So 80 non-newline chars is too long.
find . -type f ! -path '*.git*' ! -name '*~' \
    -exec grep -H -n -E '.{80}' '{}' \;

./func_dec.sh gen.c num.c buf.c gb.c eval.c ht.c regex.c curses.c fs.c

find . -type f ! -path '*.git*' -name '*.h' \
    -exec cc $flags '{}' \;

find . -type f ! -path '*.git*' -name '*.c' \
    -exec cc -c $flags '{}' \;

ld -r gen.o num.o buf.o gb.o eval.o ht.o regex.o curses.o fs.o -o toucanlib.o

cc $flags -o m4 m4.o toucanlib.o
cc $flags -o spot spot.o toucanlib.o
cc $flags -o bc bc.o toucanlib.o
cc $flags -o freq freq.o toucanlib.o
cc $flags -o tornado_dodge tornado_dodge.o toucanlib.o

mkdir -p "$install_dir"

cp -p m4 spot bc freq tornado_dodge "$install_dir"/

m4 test.m4 > .k
/usr/bin/m4 test.m4 > .k2
cmp .k .k2

# Update files
find . -type f \( -name '*.h' -o -name '*.c' \) -exec cp -p '{}' "$repo_dir" \;
