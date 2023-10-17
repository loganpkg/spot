#! /bin/sh

set -e
set -u
set -x

header="$1"
mod=$(printf '%s' "$header" | sed -E 's/\.h$//')
source="$mod".c

if [ -s "$source" ]
then
    < "$source" tr '\n' '~' | grep -E -o '~[a-zA-Z]+[^()]+\([^()]+\)~{' \
        | grep -E -v '~static ' | sed -E 's/^~(.+)~\{/\1;/' | tr '~' '\n' \
        > "$mod"_func_dec.h
fi
