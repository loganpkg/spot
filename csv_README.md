CSV Parsing
===========

If quote interpretation is turned off, then columns are separated by the
delimiter character, and lines are separated by the line separator character.

If quote interpretation is on, then each field is classified as either a
quoted or non-quoted field. A quoted field is defined as a field where the
first non-space or non-tab character is the quote character, otherwise it is
a non-quoted field.

A literal quote inside of a quoted field must be escaped by using another
quote character (two consecutive quote characters). A quote character, that
is not immediately followed by another quote character, closes the field. If
there are any non-space or non-tab characters after the closing quote and
before the next column, then this is an error. Delimiter and line separator
characters inside a quoted field are treated literally. Space and tab
characters outside of (before and after) the quoted field, but within the
same column, are removed.

Delimiter and line separator characters within a non-quoted field are
interpreted with their special meaning. No trimming of space and tab
characters occurs. Any quote characters are treated literally, and there is
no interpretation of escaped quotes (two consecutive quote characters will
remain two consecutive quote characters).

Escaped quotes are only interpreted inside of a quoted field.
