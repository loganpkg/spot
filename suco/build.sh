#! /bin/sh

#
# Copyright (c) 2025, 2026 Logan Ryan McLintock. All rights reserved.
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

######################################################################
# Configuration:
if [ "$(uname)" = Linux ]
then
    backup_prefix=/mnt/c/Users/"$(whoami)"
else
    backup_prefix="$HOME"
fi
suco_backup_dir="$backup_prefix"/OneDrive/Documents/SUCO_BACKUP
install_dir="$HOME/bin"
cc=clang
# cc=gcc
c_ops='-ansi -g -O0 -Wall -Wextra -pedantic -I. -DDEBUG'
######################################################################

c_ops="$c_ops -DSUCO_BACKUP_DIR=$suco_backup_dir"

if [ "$cc" = clang ]
then
    c_ops="-MJ cc.json -finput-charset=UTF-8 $c_ops"
else
    c_ops="-finput-charset=ascii $c_ops"
fi


wd=$(pwd)

build_dir=$(mktemp -d)
error_file="$build_dir"/error_file

export wd error_file


cc_c() {
    if [ "$cc" = clang ]
    then
        printf '[\n' > compile_commands.json
    fi

    "$cc" -c $c_ops "$1"

    if [ "$cc" = clang ]
    then
        cat cc.json >> compile_commands.json
        printf ']\n' >> compile_commands.json
        clang-tidy "$1"
    fi
}


exit_if_error() {
    if [ -s "$error_file" ]
    then
        printf "%s: Error file:\n" "$0" 1>&2
        cat "$error_file" 1>&2
        exit 1
    fi
}


fix_perms() {
    find . -type f ! -path '*.git/*' -exec chmod 600 '{}' \;
    find . -type d ! -path '*.git/*' -exec chmod 700 '{}' \;

    find . -type f ! -path '*.git/*' \
        \( -name '*.sh' -o -name '*.exp' -o -name '*.cmd' \) \
        -exec chmod 700 '{}' \;
}


fix_perms


# Copy files to build directory.
if [ "$(uname)" = Linux ]
then
    rsync -av --exclude '.git/' . "$build_dir"
else
    cpdup -I -x . "$build_dir"
fi

cd "$build_dir" || exit 1

find . -type f -name '*.sh' -exec sh -c \
    'fn="$1"; shellcheck "$1" >> "$error_file"' 'sh' '{}' \;

exit_if_error

./process_key_mappings.sh

# Update header files.
find . -type f -name '*.h' -exec sh -c '
        set -e
        set -u
        set -x
        fn="$1"
        c_fn=$(printf %s "$fn" | sed -E "s/\.h$/.c/")
        if [ ! -s "$c_fn" ]
        then
            exit 0
        fi
        line_num=$(grep -n -E "Function declarations" "$fn" | cut -d ":" -f 1)
        if [ -z "$line_num" ]
        then
           printf "%s: ERROR: Function declarations not in header: %s\n" \
                "$0" "$fn" >> "$error_file"

           exit 1
        fi
        head -n "$line_num" "$fn" > "$fn"~
        if ! clang-format -i -style=file "$c_fn"
        then
            printf "%s: ERROR: clang-format failed: %s\n" "$0" "$c_fn" \
                >> "$error_file"
        fi
        {
            grep -E -v -e "^(static |typedef )" -e ":" "$c_fn" \
                | tr -d "~" | tr "\n" "~" \
                | grep -E -o "~[a-zA-Z_][^}{]+\)~" | tr -d "\n" \
                | sed -E "s/^~//" | tr "~" "\n" | sed -E "s/\)$/);/"

            printf "\n%s\n" "#endif"
        } >> "$fn"~
        mv "$fn"~ "$fn"
    ' sh '{}' \;

exit_if_error


find . -type f \( -name '*.h' -o -name '*.c' \) -exec sh -c '
        set -e
        set -u
        set -x
        fn="$1"
        if ! clang-format -i -style=file "$fn"
        then
            printf "%s: ERROR: clang-format failed: %s\n" "$0" "$fn" \
                >> "$error_file"
        fi
    ' sh '{}' \;

exit_if_error

# Clean up.
find . -type f -name '*~' -delete


# Check for long lines.
find . -type f ! -path '*.git*' \
    \( -name '*.c' \
        -o -name '*.h' \
        -o -name '*.md' \
        -o -name '*.sh' \
        -o -name '*.exp' \
        -o -name '*.cmd' \
    \) -exec grep -H -n -E '.{80}' '{}' \; >> "$error_file" 2>&1

exit_if_error


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


"$cc" $c_ops test_buf.o buf.o int.o -o test/test_buf
"$cc" $c_ops test_input.o input.o buf.o int.o -o test/test_input
"$cc" $c_ops test_screen.o screen.o int.o -o test/test_screen
"$cc" $c_ops test_gap_buf.o gap_buf.o memmem.o screen.o input.o buf.o int.o \
    -o test/test_gap_buf

"$cc" $c_ops test_dll.o doubly_linked_list.o \
    -o test/test_dll

"$cc" $c_ops suco.o gap_buf.o memmem.o screen.o input.o buf.o int.o \
    get_time.o asprintf.o random.o -o suco


# Move source code back.
find . -type f \(    \
       -name '*.h'   \
    -o -name '*.c'   \
    -o -name '*.sh'  \
    -o -name '*.txt' \
    -o -name '*.md' \) -exec sh -c '
        set -e
        set -u
        set -x
        fn="$1"
        mv "$fn" "$wd/$fn"
    ' sh '{}' \;


cd "$wd" || exit 1
fix_perms
cd "$build_dir" || exit 1


# Move executables.
valgrind ./test/test_buf
# valgrind ./test/test_input
mv test/test_buf "$wd"/test/test_buf
mv test/test_input "$wd"/test/test_input
mv test/test_screen "$wd"/test/test_screen
mv test/test_gap_buf "$wd"/test/test_gap_buf
mv test/test_dll "$wd"/test/test_dll
mv suco "$wd"/suco

# Install.
mkdir -p "$install_dir"
cp "$wd"/suco "$install_dir"
