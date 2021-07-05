```
Copyright (c) 2021 Logan Ryan McLintock

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
```

spot text editor
================

spot is a fast text editor with many advanced features:

* Built-in flicker-free terminal graphics, no curses library is required.
* Can optionally build with a cursors library.
* Can copy and paste between multiple buffers.
* Atomic file saving on POSIX systems.
* Command multiplier prefix.
* Row and column numbers.
* Trim trailing whitespace.
* Region highlighting.
* ANSI C, easy to compile, single file, source code.

The built-in graphics use the double-buffering method to make smooth screen
changes. The text memory uses the gap buffer method for efficient editing.

Install
-------

To compile simply run:
```
$ cc -g -O3 spot.c && mv a.out spot
```
or
```
> cl /Ot spot.c
```
and place the executable somewhere in your PATH.

spot can optionally be compiled with a curses library by setting
`USE_CURSES` to 1, followed by:
```
$ cc -g -O3 spot.c -lncurses && mv a.out spot
```
or by commands similar to this, after downloading and extracting PDCurses:
```
> cd C:\Users\logan\Documents\PDCurses-3.9\PDCurses-3.9\wincon
> nmake -f Makefile.vc
> cd C:\Users\logan\Documents\spot
> cl /Ot spot.c pdcurses.lib User32.Lib AdvAPI32.Lib ^
  /I C:\Users\logan\Documents\PDCurses-3.9\PDCurses-3.9 ^
  /link /LIBPATH:C:\Users\logan\Documents\PDCurses-3.9\PDCurses-3.9\wincon
```
Please note that PDCurses does not handle terminal size changes.

To use
------
```
$ spot [file...]
```

Keybindings
-----------

The keybindings can be seen by pressing ESC followed by question mark,
which will display the following:

```
spot keybindings
^ means the control key, and ^[ is equivalent to the escape key.
RK denotes the right key and LK the left key.
Commands with descriptions ending with * take an optional command
multiplier prefix ^U n (where n is a positive number).
^[ ?   Display keybindings in new gap buffer
^b     Backward char (left)*
^f     Forward char (right)*
^p     Previous line (up)*
^n     Next line (down)*
^h     Backspace*
^d     Delete*
^[ f   Forward word*
^[ b   Backward word*
^[ u   Uppercase word*
^[ l   Lowercase word*
^q hh  Quote two digit hexadecimal value*
^a     Start of line (home)
^e     End of line
^[ <   Start of gap buffer
^[ >   End of gap buffer
^[ m   Match bracket
^l     Level cursor and redraw screen
^2     Set the mark
^g     Clear the mark or escape the command line
^w     Wipe (cut) region
^o     Wipe region appending on the paste gap buffer
^[ w   Soft wipe (copy) region
^[ o   Soft wipe region appending on the paste gap buffer
^k     Kill (cut) to end of line
^[ k   Kill (cut) to start of line
^y     Yank (paste)
^t     Trim trailing whitespace and clean
^s     Search
^[ n   Search without editing the command line
^x i   Insert file at cursor
^x ^F  Open file in new gap buffer
^r     Rename gap buffer
^x ^s  Save current gap buffer
^x LK  Move left one gap buffer
^x RK  Move right one gap buffer
^[ !   Close current gap buffer without saving
^x ^c  Close editor without saving any gap buffers
```

Enjoy,
Logan =)_
