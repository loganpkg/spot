<!--

Copyright (c) 2023 Logan Ryan McLintock

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

-->

m4 monorepo
===========

Welcome to the _m4 monorepo_ where multiple applications share code via
modules.

The applications are:
* [m4](#m4): An implementation of the m4 macro processor,
* [spot](#spot): The text editor, and
* [bc](#bc): A basic calculator.

spot and m4 utilise the built-in regular expression module which understands
the [regex syntax](#regex-syntax) listed in a later section.


Install
-------

This software is cross-platform and has been written in ANSI C with the minimum
use of non-standard libraries. To install, edit one of the scripts below
(depending upon your operating system) to set `install_dir`.
Then simply run:
```
./install.sh
```
or
```
< winstall.txt cmd
```
(actually type the `<` character) and make sure `install_dir` is included in
your `PATH`.

m4
==

m4 is a general purpose macro processor. It performs text replacement, similar
to the C preprocessor, although it is not tied to any specific language. It
performs programmatic find-and-replace on text, but can also perform other
tasks, such as executing shell commands.

How m4 works
------------

m4 has two classes of macros; built-in macros and user-defined macros.
Built-in macros are written in C and can only be added or modified by editing
the source code and recompiling. User-defined macros are written in the m4
language and are added using the `define` macro. Macros from either class can
be undefined (removed), but built-in macros cannot come back with their old
built-in nature (although you can reuse the name for a new user-defined macro).

m4 reads word-by-word from a centralised input buffer. (If quote mode is not
activated) each word is looked up in a hash table to see if it is the name of
a macro. If it is then the macro is pushed onto the stack. If the macro takes
arguments, then these will be collected. When the macro is finished, for
user-defined macros, the arguments are substituted into the placeholders in
the macro definition and then the resultant text is pushed back into the
input. Built-in macros usually perform some other operation on the arguments,
and some of them also push the result back into the input.

Nested macro calls are handled by the stack. While collecting the arguments of
one macro, another macro may be encountered. m4 deals with macros immediately.
It will add the new macro to the stack and start collecting its arguments.
Only once processing of the inner macro is done, will execution return to the
outer macro. The inner macro may push its result back into the input which
will then be read and rechecked for macros, until eventually the text has been
fully expanded and ends up in the argument collection of the outer macro. This
gives m4 its powerful recursive nature.

m4 also has flexible output. At any given time the _output_ is either the
argument collection of a macro, or one of the eleven different diversions
(output buffers). Diversion -1 is discarded and is useful when defining a lot
of macros. Diversion 0 is regularly flushed to `stdout` and is the default.
Diversions 1 to 9 are temporary storage. Diversions 1 to 9 can be written to
file, which is very useful when writing dynamic code. They can also be
undiverted into each other and will all be flushed to `stdout` if the program
exits without error.

m4 uses quotes to suppress the expansion of macros by passing words directly
to the output. Quotes can be nested. When entering quote mode the left quote
is eaten, and when exiting quote mode the right quote is eaten, but quotes
in-between are retained. Due to the recursive nature of m4, text is often
evaluated multiple times, and each time the outer layer of quotes will be
striped.

m4 only checks for macros when reading from the _input_ in non-quote mode.
During argument collection, quote mode prevents commas from being interpreted
as argument separators (as do added parentheses). Once argument collection
is finished, quotes have no effect on the substitution of collected arguments
into their placeholders, this will occur irrespective of the depth of
quotation.

Example
-------

In the example below, the quote characters are changed to `[` and `]`. Then a
new macro called `world` is created with a defining text of `cool`. `world` is
then shown to expand to `cool`, as expected.

It superficially looks like `x` is defined as `[[hello $1]]`, however, as
`[[hello $1]]` is read from the _input_, quote mode is entered and exited,
stripping off the outer layer of quotes. As shown by `dumpdef`, `x` is actually
defined with a single layer of quotes, `[hello $1]`.

`x` is called with what appears like an argument of `[world]`. However, as
`[world]` is read from the input, quote mode is entered and exited, stripping
off the quotes, resulting in a gathered argument of `world` (which was not
looked up in the hash table due to the quote mode).

The argument, `world` is substituted into the placeholder `$1`, interestingly,
unaffected by the quotes in the _definition_! The result, `[hello world]`,
(which cannot be viewed directly) is pushed back into the input. When this
is re-read, quote mode is entered and exited again, stripping off the quotes
and preventing `world` from being expanded, leaving the output as simply
`hello world`.

Quoting the input to `x` and the output from `x` gave no opportunity for
`world` to expand to `cool`.

```m4
changequote([, ])

define(world, cool)

world
cool
define(x, [[hello $1]])

dumpdef([x])
x: [hello $1]

x([world])
hello world
m4exit
```

I strongly recommend learning m4. For a small investment in learning it
provides a powerful tool that is free from a lot of the limitations imposed by
many programming languages.

Built-in macros
---------------

I will now introduce the built-in macros.

```m4
define(macro_name, macro_def)
```
`define` is used to create user-defined macros. If the macro already exists,
then the old macro will be replaced, even if it is a built-in macro (which
loses the ability to bring it back). Please note that quotes are needed
when replacing a macro, otherwise the macro will expand during argument
collection, prior to the `define` macro being executed. Macro names must start
with an alpha character or underscore followed by none or more alpha, digit or
underscore characters. The macro definition is the text that the macro will
expand into. It can take argument placeholders, `$0` to `$9`. `$0` is the
macro name. `$1` to `$9` are the arguments collected when the macro is called.
Omitted arguments are treated as empty strings.

```m4
undefine(`macro_name')
```
`undefine` removes a macro from the hash table. It is necessary to quote the
macro name. Built-in macros cannot be retrieved once undefined.

```m4
changequote(left_quote, right_quote)
```
Sets the left and right quote characters. Please note that they must be
different, single graph characters. The defaults of backtick and
single quote are restored when called without arguments. It is normally a good
idea to pick characters that are not a commonly used in any downstream
programming language. I often change the quote characters to `[` and `]` or
`@` and `~`, as single quotes are commonly used.

```m4
divert or divert(div_num)
```
`divert` changes the active diversion. When called without arguments, the
default diversion of 0 is used (which is regularly flushed to `stdout`).
Diversion -1 is discarded. It is often used when defining multiple macros, as
the remaining newline characters are typically not wanted in the output.

```m4
undivert or undivert(div_num, filename, ...)
```
`undivert` appends the contents of a diversion or file into the current active
diversion. Undiverted diversions are emptied. A diversion cannot be undiverted
into itself, and diversion -1 cannot be undiverted (as it is discarded). When
called without arguments (which is only allowed from diversion 0), diversions
1 to 9 are all undiverted in order. It is important to note that no processing
occurs during this, macros are not expanded.

```m4
writediv(div_num, filename)
```
`writediv` empties the specified diversion to file. Creates missing directories
in the file path.

```m4
divnum
```
`divnum` pushes the active diversion number into the input.

```m4
include(filename)
```
`include` pushes the contents of a file into the input. Macros will be
processed.

```m4
dnl
```
`dnl` deletes to (and including) the next newline character. Often used for
single-line comments or for removing the newline character after a macro
definition.

```m4
tnl(str)
```
`tnl` trims trailing newline characters from the end of the first argument.

```m4
regexrep(text, regex_find, replace[, nl_insensitive[, verbose]])
```
`regexrep` searches text for a regex pattern and replaces the matches.
If the fourth argument is 1, then newline insensitive matching occurs.
If verbose is 1, then the posfix form of the expression and the
nondeterministic finite automaton (NFA) structure are printed to `stderr`.

```m4
ifdef(`macro_name', `when_defined', `when_undefined')
```
`ifdef` checks to see if the first argument is a macro, and if so, pushes the
second argument back into the input. Otherwise, the third argument is pushed
back into the input. The macro name should be quoted to prevent it from
expanding during argument collection. Also, importantly, macros will be
expanded and processed immediately during argument collection, _before_ the
branch in logic. So, the second and third arguments should also be quoted.

```m4
ifelse(A, B, `when_same', `when_different')
```
`ifelse` compares the first and second arguments (after any expansions that
occur during collection), and if they are equal, pushes the third argument
back into the input. Otherwise, the fourth argument is pushed back into the
input. Remember that arguments will be expanded and processed during argument
collection, which occurs _before_ the branch in logic. So, it is a good idea to
quote the third and fourth arguments.

```m4
dumpdef or dumpdef(`macro_name', )
```
`dumpdef` prints the definitions of the marcos specified in the arguments
(which should be quoted) to `stderr`. When called without arguments, all
macro definitions are printed. Useful when debugging.

```m4
errprint(error_message)
```
`errprint` prints a message to `stderr`.

```m4
incr(number)
```
`incr` increments a number. The result is pushed into the input.

```m4
eval(math[, verbose])
```
`eval` evaluates an arithmetic expression. It understands `(`, `)`, `^`,
`*`, `/`, `%` (modulus), and _unary_ and binary `+` and `-`. Works with signed
_long_ integers. If verbose is 1, then the postfix form of the expression is
printed to `stderr`.

```m4
sysval
```
`sysval` pushes the return value of the last shell command run via `esyscmd`
into the input.

```m4
esyscmd(shell_command)
```
`esyscmd` runs an operating system specific shell command and reads the
`stdout` of that command into the input.

```m4
m4exit(exit_value)
```
`m4exit` allows the user to request early termination of m4, specifying the
desired exit value in the first argument (with the default being zero when
called with no arguments). Please note that the specified exit value will be
overwritten with 1 if an error occurs.

```m4
recrm(path)
```
`remove` recursively removes a path if it exists.


spot
====

spot is a cross-platform text editor that has been written in ANSI C with the
minimum use of non-standard libraries.

It uses double-buffering to display flicker-free graphics without using any
curses library.

Gap buffers are used to edit the text, which are very efficient for most
operations. A nice balance has been achieved between optimisation, features,
and code maintainability.

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
| `^s`    | Exact forward search (moves cursor to start of the match) |
| `^z`    | Regex forward search (moves cursor to after the match)    |
| `^r`    | Regex replace region*                                     |
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

`*` Regex replace region syntax is `_find_replace` where the first character
(in this example, `_`) is the delimiter. The anchors, `^` and `$` are relative
to the region, not the buffer.

`^` Text editor will exit if it is the last gap buffer.


bc
==

bc is a cross-platform basic calculator. It reads from `stdin` and works with
signed _long_ integers. It understands `(`, `)`, `^`, `*`, `/`, `%` (modulus),
and _unary_ and binary `+` and `-`.


Regex syntax
============

Regular expression syntax:
* Backslash outside of character set:
    - `\t` Tab.
    - `\n` Line feed.
    - `\r` Carriage return.
    - `\0` Null character.
    - `\xAA` Two digit hex value.
    - `\c` Any other character, `c`, is interpreted literally.
* `[^a-b]` Character set.
* `(` and `)` Grouping.
* Anchors:
    - `^` Start of line.
    - `$` End of line.
* Repetition operators:
    - `*` Zero or more.
    - `+` One or more.
    - `?` Zero or one.
* `|` Alternate.


Character sets
--------------

* Most characters are treated literally inside a character set.
* `^` immediately after the opening `[` negates the set and is not considered
as a character.
* `-` _between_ characters creates an inclusive range.
* Characters sets cannot be empty.
* The first character is always treated literally, so `[^]]` is
the set of all characters except for `]`.

Enjoy,
Logan =)_
