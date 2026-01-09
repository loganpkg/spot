#! /bin/sh

#
# Copyright (c) 2023, 2024 Logan Ryan McLintock. All rights reserved.
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
