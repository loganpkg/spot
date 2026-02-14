#! /bin/sh

#
# Copyright (c) 2023, 2024, 2026 Logan Ryan McLintock. All rights reserved.
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

# Install sh script for spot project.

# shellcheck disable=SC2086

set -e
set -u
set -x


# Config.
default_prefix=$HOME
cflags='-ansi -g -Og -Wno-variadic-macros -Wall -Wextra -pedantic -I .'
apps='spot m4 bc freq tornado_dodge'


make_executable() {
    ex_nm=$(printf %s "$1" | sed -E 's/\.o$//')
    cc $cflags "$@" toucanlib.o -o "$ex_nm"
}


export cflags


find . ! -path '*.git/*' -type f -name '*.c' -exec sh -c '
    set -e
    set -u
    set -x
    cc -c $cflags "$1"
' sh '{}' \;


ld -r gen.o num.o buf.o gb.o eval.o ht.o toco_regex.o fs.o -o toucanlib.o


make_executable spot.o curses.o
make_executable m4.o
make_executable bc.o
make_executable freq.o
make_executable tornado_dodge.o curses.o


set +u
if [ -z "$PREFIX" ]
then
    PREFIX=$default_prefix
fi
set -u


mkdir -p "$PREFIX"/bin

cp -p $apps "$PREFIX"/bin

rm -f -- *.o *.obj *.lib *.ilk *.pdb *.exe $apps
