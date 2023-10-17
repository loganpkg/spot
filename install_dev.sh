#! /bin/sh

set -e
set -u
set -x

install_dir="$HOME"/bin
repo_dir="$(pwd)"
build_dir="$(mktemp -d)"

find . -type f ! -path '*.git*' -exec cp -p '{}' "$build_dir" \;

cd "$build_dir" || exit 1

find . -type f \( -name '*.h' -o -name '*.c' \) -exec indent -nut -kr -bad '{}' \;

find . -type f ! -path '*.git*' -name '*.h' -exec ./func_dec.sh '{}' \;

find . -type f ! -path '*.git*' -name '*.h' ! -name '*_func_dec.h' -exec cc -ansi -Wall -Wextra -pedantic '{}' \;

find . -type f ! -path '*.git*' -name '*.c' -exec cc -c -ansi -Wall -Wextra -pedantic '{}' \;

cc -ansi -Wall -Wextra -pedantic -o m4 m4.o num.o buf.o ht.o
cc -ansi -Wall -Wextra -pedantic -o spot spot.o num.o gb.o

cp -p m4 "$install_dir"
cp -p spot "$install_dir"

m4 test.m4 > .k
/usr/bin/m4 test.m4 > .k2
cmp .k .k2

# Update files
find . -type f \( -name '*.h' -o -name '*.c' \) -exec cp -p '{}' "$repo_dir" \;
