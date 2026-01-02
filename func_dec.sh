#! /bin/sh

#
# Copyright (c) 2023, 2024 Logan Ryan McLintock. All rights reserved.
#
# The contents of this file are subject to the
# Common Development and Distribution License (CDDL) version 1.1
# (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License in
# the LICENSE file included with this software or at
# https://opensource.org/license/cddl-1-1
#
# NOTICE PURSUANT TO SECTION 4.2 OF THE LICENSE:
# This software is prohibited from being distributed or otherwise made
# available under any subsequent version of the License.
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
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
