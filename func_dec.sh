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

source="$1"
header='toucanlib.h'

sed -E '/^\/\* Function declarations \*\/$/q' "$header" > "$header"~

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
