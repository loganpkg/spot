<!--

Copyright (c) 2023 Logan Ryan McLintock

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

-->

spot
====

spot is a cross-platform text editor that has been written in ANSI C with the
minimum use of non-standard libraries.

It uses double-buffering to display flicker-free graphics without using any
curses library.

Gap buffers are used to edit the text, which are very efficient for most
operations. A nice balance has been achieved between optimisation, features,
and code maintainability.

To _install_, simply compile `spot.c` and place the executable somewhere in
your `PATH`.

The region is the area between the cursor and the mark, with whichever appears
first included in the region and whichever appears last excluded from the
region. It is cleared by editing commands, and navigational commands are used
to change its coverage.

The status bar displays `!` if the last command failed, followed by `*` if the
buffer has been modified. The filename associated with the buffer is presented
next, followed by the current row and column number in brackets. The hex value
of the char under the cursor (which may be in the command line) is displayed.
Finally, the if the last command included a shell command which succeeded (the
process terminated normally), then the exit status is displayed.

The command line is at the bottom of the window and is used for _two-step_
commands that require user input. Most single-step commands work inside the
command line.

The keybindings are listed below. `^a` means pressing `Ctrl` plus `a`.
`LK` denotes the left key, and `RK` denotes the right key.

| Keys    | Command                                                   |
| :------ | :-------------------------------------------------------- |
| `^b`    | Left character                                            |
| `^f`    | Right character                                           |
| `^p`    | Up line                                                   |
| `^n`    | Down line                                                 |
| `^d`    | Delete character                                          |
| `^h`    | Backspace character                                       |
| `^a`    | Start of line                                             |
| `^e`    | End of line                                               |
| `^2`    | Set mark                                                  |
| `^g`    | Clear mark, or exit command line                          |
| `^l`    | Centre cursor on the screen and redraw graphics           |
| `^w`    | Cut region                                                |
| `^y`    | Paste                                                     |
| `^k`    | Cut to end of line                                        |
| `^o`    | Insert shell command of logical line under the cursor +   |
| `^t`    | Trim trailing white-space and remove non-printable chars  |
| `^s`    | Forward search                                            |
| `^r`    | Replace region (find and replace confined to the region)* |
| `^u`    | Go to line number                                         |
| `^q`    | Insert hex                                                |
| `Esc b` | Left word                                                 |
| `Esc f` | Right word                                                |
| `Esc l` | Lowercase word                                            |
| `Esc u` | Uppercase word                                            |
| `Esc k` | Cut to start of line                                      |
| `Esc m` | Match bracket `<>`, `[]`, `{}`, or `()`                   |
| `Esc n` | Repeat last search                                        |
| `Esc w` | Copy region                                               |
| `Esc !` | Remove current gap buffer without saving ^                |
| `Esc =` | Rename gap buffer                                         |
| `Esc $` | Insert shell command from the command line                |
| `Esc <` | Start of gap buffer                                       |
| `Esc >` | End of gap buffer                                         |
| `^x ^c` | Close editor without saving any buffers                   |
| `^x ^s` | Save the current buffer only                              |
| `^x ^f` | New gap buffer                                            |
| `^x i`  | Insert file                                               |
| `^x LK` | Move left one gap buffer                                  |
| `^x RK` | Move right one gap buffer                                 |


`+` The _logical_ line under the cursor is formed by joining neighbouring lines
that end in a backslash, to accommodate for long lines. These end-of-line
backslashes are removed from the logical line, as are `\n` characters.
`2>&1` is added to the end of the logical line, to capture `stderr` under most
situations. If some `stderr` text comes through uncaptured, then it can be
cleared by redrawing the screen (`^l`).

`*` Replace region syntax is `find|replace` where `\` is used as the escape
character and; `\n` is a line feed, `\t` is a tab, `\\` is a literal backslash,
and `\|` is a literal pipe character (instead of being interpreted as the
delimiter).

`^` Text editor will exit if it is the last gap buffer.

Enjoy,
Logan =)_
