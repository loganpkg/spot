#! /bin/sh

set -e
set -u
set -x

# Configuration
install_dir="$HOME"/bin
flags='-ansi -Wall -Wextra -pedantic'

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
    -exec "$indent" -nut -kr -bad '{}' \; 2> err

if [ -s err ]
then
    cat err
    exit 1
fi

find . -type f ! -path '*.git*' \
    -exec grep -H -n -E '.{80}' '{}' \;

find . -type f ! -path '*.git*' -name '*.h' \
    -exec ./func_dec.sh '{}' \;

find . -type f ! -path '*.git*' -name '*.h' ! -name '*_func_dec.h' \
    -exec cc $flags '{}' \;

find . -type f ! -path '*.git*' -name '*.c' \
    -exec cc -c $flags '{}' \;

cc $flags -o m4 m4.o gen.o num.o buf.o eval.o ht.o
cc $flags -o spot spot.o gen.o num.o gb.o
cc $flags -o bc bc.o gen.o num.o buf.o eval.o

cc $flags -o regex regex.o

cp -p m4 spot bc regex "$install_dir"

m4 test.m4 > .k
/usr/bin/m4 test.m4 > .k2
cmp .k .k2

# Update files
find . -type f \( -name '*.h' -o -name '*.c' \) -exec cp -p '{}' "$repo_dir" \;
