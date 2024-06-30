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

if [ "$#" -lt 2 ]
then
    printf 'Usage: %s header_file source_file...\n' "$0" 1>&2
    exit 1
fi

header="$1"
shift

sed -E '/^\/\* Function declarations \*\/$/q' "$header" > "$header"~

# For each remaining arg
for source
do
    if [ -s "$source" ]
    then
        < "$source" tr '\n' '~' | grep -E -o '~[a-zA-Z_][^()~]+\([^()]+\)~\{' \
            | grep -E -v '~static ' | sed -E 's/^~(.+)~\{/\1;/' | tr '~' '\n' \
            >> "$header"~
    fi
done

printf '\n#endif\n' >> "$header"~

chmod 600 "$header"~

mv "$header"~ "$header"
