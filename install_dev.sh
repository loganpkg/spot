#! /bin/sh

set -e
set -u
set -x

flags='-ansi -Wall -Wextra -pedantic'
install_dir="$HOME"/bin

if [ "$(uname)" = Linux ]
then
    indent=indent
else
    indent=gindent
fi

repo_dir="$(pwd)"
build_dir="$(mktemp -d)"

find . -type f ! -path '*.git*' -exec cp -p '{}' "$build_dir" \;

cd "$build_dir" || exit 1

find . -type f \( -name '*.h' -o -name '*.c' \) \
    -exec "$indent" -nut -kr -bad '{}' \;

find . -type f ! -path '*.git*' \
    -exec grep -H -n -E '.{80}' '{}' \;

find . -type f ! -path '*.git*' -name '*.h' \
    -exec ./func_dec.sh '{}' \;

find . -type f ! -path '*.git*' -name '*.h' ! -name '*_func_dec.h' \
    -exec cc $flags '{}' \;

find . -type f ! -path '*.git*' -name '*.c' \
    -exec cc -c $flags '{}' \;

cc $flags -o m4 m4.o num.o buf.o ht.o
cc $flags -o spot spot.o num.o gb.o

cp -p m4 "$install_dir"
cp -p spot "$install_dir"

m4 test.m4 > .k
/usr/bin/m4 test.m4 > .k2
cmp .k .k2

# Update files
find . -type f \( -name '*.h' -o -name '*.c' \) -exec cp -p '{}' "$repo_dir" \;
