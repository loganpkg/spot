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
| `Esc =` | Rename gap buffer                                         |
| `Esc <` | Start of gap buffer                                       |
| `Esc >` | End of gap buffer                                         |
| `^x ^c` | Close editor without saving any buffers                   |
| `^x ^s` | Save the current buffer only                              |
| `^x ^f` | New gap buffer                                            |
| `^x i`  | Insert file                                               |
| `^x LK` | Move left one gap buffer                                  |
| `^x RK` | Move right one gap buffer                                 |

* Replace region syntax is `find|replace` where `\` is used as the escape
character and; `\n` is a line feed, `\t` is a tab, `\\` is a literal backslash,
and `\|` is a literal pipe character (instead of being interpreted as the
delimiter).

Enjoy,
Logan =)_
