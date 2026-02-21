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

# shellcheck disable=SC2086

set -e
set -u
set -x


#################
# Configuration #
#################
install_dir="$HOME"/bin
cc=clang
# cc=gcc
flags='-ansi -g -Og -Wno-variadic-macros -Wall -Wextra -pedantic'
# Change to N to use ncurses
use_built_in_curses=Y
#################


cc_c() {
    if [ "$cc" = clang ]
    then
        printf '[\n' > compile_commands.json
    fi

    "$cc" -c $flags "$1"

    if [ "$cc" = clang ]
    then
        cat cc.json >> compile_commands.json
        printf ']\n' >> compile_commands.json
        clang-tidy "$1"
    fi
}


if [ "$cc" = clang ]
then
    flags="-MJ cc.json -finput-charset=UTF-8 $flags"
else
    flags="-finput-charset=ascii $flags"
fi


if [ "$use_built_in_curses" = Y ]
then
    # To look in the current working directory for <curses.h>
    flags="$flags -I ."
fi


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
    -exec clang-format -i -style=file '{}' \; 2> err

if [ -s err ]
then
    cat err
    exit 1
fi


# Max of 79 chars per line plus the \n, making a total of 80.
# So 80 non-newline chars is too long.
find . -type f ! -path '*.git*' ! -name '*~' ! -name 'tornado_dodge.txt' \
    -exec grep -H -n -E '.{80}' '{}' \; > err 2>&1

if [ -s err ]
then
    cat err
    exit 1
fi


./func_dec.sh toucanlib.h gen.c num.c buf.c gb.c eval.c ht.c \
    toco_regex.c fs.c

./func_dec.sh curses.h curses.c


tmp=$(mktemp)

find . -type f ! -path '*.git*' -name '*.c' > "$tmp"

if [ "$(uname)" = Linux ] || [ "$cc" != gcc ]
then
    find . -type f ! -path '*.git*' -name '*.h' >> "$tmp"
fi

while IFS='' read -r x
do
    printf 'Compiling: %s\n' "$x"
    cc_c "$x"
done < "$tmp"

ld -r gen.o num.o buf.o gb.o eval.o ht.o toco_regex.o fs.o -o toucanlib.o


"$cc" $flags -o m4 m4.o toucanlib.o
"$cc" $flags -o bc bc.o toucanlib.o
"$cc" $flags -o freq freq.o toucanlib.o


if [ "$use_built_in_curses" = Y ]
then
    "$cc" $flags -o spot spot.o curses.o toucanlib.o
    "$cc" $flags -o tornado_dodge tornado_dodge.o curses.o toucanlib.o
else
    "$cc" $flags -o spot spot.o toucanlib.o -lncurses
    "$cc" $flags -o tornado_dodge tornado_dodge.o toucanlib.o -lncurses
fi

ldd spot tornado_dodge

mkdir -p "$install_dir"

cp -p m4 spot bc freq tornado_dodge "$install_dir"/

m4 test.m4 > .k
/usr/bin/m4 test.m4 > .k2
cmp .k .k2

# Update files
find . -type f \( -name '*.h' -o -name '*.c' \) -exec cp -p '{}' "$repo_dir" \;
