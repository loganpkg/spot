#! /bin/sh

#
# Copyright (c) 2026 Logan Ryan McLintock. All rights reserved.
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

# Process key mappings for suco.

set -e
set -u
set -x

tmp_km=$(mktemp)
grep -E -v '^(#|$)' key_mappings.txt > "$tmp_km"

rm -f .key_sequence_records.txt .key_func_pointer_records.txt

tmp_md_table=$(mktemp)
printf '| Suco function ^| Key sequence ^|
| --- ^| --- ^|\n' > "$tmp_md_table"

while IFS='' read -r line
do
    c_func=$(printf %s "$line" | cut -d '|' -f 1)
    key_str=$(printf %s "$line" | cut -d '|' -f 2-)

    key_seq=$(printf %s "$key_str" \
        | sed -E -e "s/(^| )([^ ])( |$)/\1'\2'\3/g" -e 's/ /, /g')

    printf '        { { %s }, ID },\n' "$key_seq" >> .key_sequence_records.txt

    printf '        &%s,\n' "$c_func" >> .key_func_pointer_records.txt

    printf '| %s ^| %s ^|\n' "$c_func" "$key_str" >> "$tmp_md_table"
done < "$tmp_km"

column -s '^' -t "$tmp_md_table" > .key_mappings_table.md
