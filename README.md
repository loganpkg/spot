<!--

Copyright (c) 2023, 2024 Logan Ryan McLintock

Permission to use, copy, modify, and/or distribute this software for any
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

spot monorepo
=============

Welcome to the _spot monorepo_ where multiple cross-platform applications share
code via _toucanlib_.

The applications are:
* [spot](#spot): The text editor,
* [m4](#m4): A POSIX compliant implementation of the m4 macro processor,
* [bc](#bc): A basic calculator, and
* [freq](#freq): Determines the character frequency in a file.

toucanlib has a monolithic interface to make usage easy, but multiple modules
to make maintenance easy. Currently, the modules include:
* gen: Generic functions,
* num: Number functions,
* buf: Various buffers,
* gb: Gap buffers,
* eval: Evaluate arithmetic expression,
* ht: Hash table,
* curses: Curses (terminal graphics), and
* fs: File system related functions.

Install
-------

This software is cross-platform and has been written in ANSI C with the minimum
use of non-standard libraries. To install, edit one of the scripts below
(depending upon your operating system) to set `install_dir`.
Then simply run:
```
$ make
$ PREFIX="$HOME" make install
$ make clean
```
or
```
> nmake /F nMakefile
> set PREFIX=%HOMEDRIVE%%HOMEPATH%&& nmake /F nMakefile install
> nmake /F nMakefile clean
```

Make sure `PREFIX/bin` is included in your `PATH`.


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
`^[` is the `Ctrl` key plus `[`, but is also generated by pressing the
`Esc` key. `LK` denotes the left key, and `RK` denotes the right key.

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
| `^s`    | Exact forward search (moves cursor to start of the match) |
| `^z`    | Regex forward search (moves cursor to after the match)    |
| `^r`    | Regex replace region *                                    |
| `^u`    | Go to line number                                         |
| `^q`    | Insert hex                                                |
| `^[ b`  | Left word                                                 |
| `^[ f`  | Right word                                                |
| `^[ l`  | Lowercase word                                            |
| `^[ u`  | Uppercase word                                            |
| `^[ k`  | Cut to start of line                                      |
| `^[ m`  | Match bracket `<>`, `[]`, `{}`, or `()`                   |
| `^[ n`  | Repeat last search                                        |
| `^[ w`  | Copy region                                               |
| `^[ !`  | Remove current gap buffer without saving ^                |
| `^[ =`  | Rename gap buffer                                         |
| `^[ $`  | Insert shell command from the command line                |
|``^[ ` ``| Insert shell command of logical line under the cursor +   |
| `^[ <`  | Start of gap buffer                                       |
| `^[ >`  | End of gap buffer                                         |
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


m4
==

This version of m4 is cross-platform (runs natively) and complies to
POSIX standard IEEE Std 1003.1-2017.

m4 is a general purpose macro processor. It performs text replacement, similar
to the C preprocessor, although it is not tied to any specific language. It
performs programmatic find-and-replace on text, but can also perform other
tasks, such as executing shell commands.

Usage
-----

```sh
m4 [-s] [-D macro_name[=macro_def]] ... [-U macro_name] ... file ...
```
Where:
* `-s` prints `#line` directive for the C preprocessor.
* `-D` defines the macro specified in the next argument, with optionally,
    the macro's definition given after a separating `=` character.
* `-U` undefines the macro name specified in the next argument.
* `file` is a list of regular files, with `-` denoting `stdin`. If no files
    are specified, then `stdin` is read by default.

How m4 works
------------

m4 has two classes of macros; built-in macros and user-defined macros.
Built-in macros are written in C and can only be added or modified by editing
the source code and recompiling. However you can undefine (remove) them and
you can make renamed copies of them that behave exactly the same.
If you undefine a built-in macro (and you don't have a renamed copy), then
you cannot get it back.

User-defined macros are written in the m4 language and are added using the
`define` macro.

m4 reads word-by-word from a centralised input buffer. If you are not in
a comment and quote mode is not activated, then each word is looked up in
a hash table to see if it is the name of a macro.
If it is then the macro is pushed onto the stack. If the macro takes
arguments, then these will be collected. When the macro is finished, for
user-defined macros, the arguments are substituted into the placeholders in
the macro definition and then the resultant text is pushed back into the
input. Built-in macros usually perform some other operation on the arguments,
and some of them also push the result back into the input.

Nested macro calls are handled by the macro call stack. While collecting the
arguments of one macro, another macro may be encountered.
m4 deals with macros immediately.
It will add the new macro to the stack and start collecting its arguments.
Only once processing of the inner macro is done, will execution return to the
outer macro. The inner macro may push its result back into the input which
will then be read and rechecked for macros, until eventually the text has been
fully expanded and ends up in the argument collection of the outer macro. This
gives m4 its powerful recursive nature, but also makes it confusing for
beginners.

m4 also has flexible output. At any given time the _output_ is either the
argument collection of a macro, or one of the eleven different diversions
(output buffers). Diversion -1 is discarded and is useful when defining a lot
of macros. Diversion 0 is regularly flushed to `stdout` and is the default.
Diversions 1 to 9 are temporary storage. Diversions 1 to 9 can be written to
file, which is very useful when writing dynamic code (this is an extension to
the POSIX standard). They can also be undiverted into each other and will all
be flushed to `stdout` if the program exits without error.

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

In the example below, the quotes are changed to `[` and `]`. Then a
new macro called `world` is created with a defining text of `cool`. `world` is
then shown to expand to `cool`, as expected.

```m4
changequote([, ])

define(world, cool)

world
cool
define(x, [[hello $1]])

dumpdef([x])
User-def: x: [hello $1]

x([world])
hello world
m4exit
```

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

I strongly recommend learning m4. For a small investment in learning it
provides a powerful tool that is free from a lot of the limitations imposed
by many programming languages.

Built-in macros
---------------

I will now introduce the built-in macros. All built-in macros that *require*
arguments exhibit pass-through, whereby the macro name is simply printed to
the output when it is called without arguments.

```m4
changequote[(left_quote, right_quote)]
```
Sets the left and right quote strings. Please note that they should be different,
non-empty strings that should only contain graph, non-comma, non-parentheses
characters, in order to function in a nice way with the m4 design.
It is normally a good idea to pick strings that are not a commonly
used in any downstream programming language, such as `<[` and `]>`.
When called without arguments, the default quotes of backtick and apostrophe
are restored.

```m4
changecom[(left_comment[, right_comment])]
```
`changecom` changes the default left and right comment strings from `#` and the
newline to the ones specified. If called with only one argument, then the right
comment string defaults to the newline. When called without any arguments, then
comments are disabled. Text inside comments, and the comments themselves, are
passed though to the output. No interpretation is performed on text within
comments, besides checking for the right comment string to know when to exit
the comment.

```m4
define(macro_name, macro_def)
```
`define` creates a new user-defined macro (if it does not already exist), or
*updates* the current history of an existing macro, retaining any prior history
that may exist (but not-preserving the current history).

If the current history is a built-in macro, then it will still be updated, but
the ability to bring back the built-in nature will be lost (unless you have
another copy).

You can make a renamed copy of a built-in macro, which then acts exactly the
same as an orginal built-in macro. To do this you need to used `defn`,
for example:

```m4
define(x, defn(`m4exit'))

dumpdef(`x')
Built-in: x[(exit_value)]

x
```

This also works with `pushdef`.

Please note that quotes are usually wanted when replacing a macro, as otherwise
the macro will expand during argument collection, prior to the `define` macro
being executed.

Macro names must start with an alpha character or underscore followed
by none or more alpha, digit or underscore characters.

The macro definition is the text that the macro will expand into.
It can take argument placeholders, `$0` to `$9`. `$0` is the macro name.
`$1` to `$9` are the arguments collected when the macro is called.
A macro can take any number of arguments, but only the first 9 can be
referenced individually. $# expands to the number of collected arguments,
and $* to a comma-separated list of all collected arguments, and $@ to
individually quotes, comma-separated list of all collected arguments.

```m4
pushdef(macro_name, macro_def)
```
`pushdef` acts like `define`, except that if the macro already exists, then
the new definition will be stacked onto the history stack for that macro.
The current history will be preserved, and will become the macro definition
immediately below the new definition in the history stack.

`pushdef` can be used to make a renamed copy of a built-in macro by using
`defn` (this also works with user-defined macros).

```m4
pushdef(x, defn(`divnum'))

x
0
m4exit
```

```m4
defn(macro_name)
```
`defn` is often used to make a renamed copy of a macro.
For user-defined macros it pushes the quoted definition into the input.
For built-in macros, it passes back the C function pointer (its *definition*)
to the parent macro, but only when it aligns to the second argument
of `define` or `pushdef` (or renamed copies of these).

```m4
divert[(div_num)]
```
`divert` changes the active diversion. m4 commences in diversion 0,
which is regularly flushed to `stdout`. Diversion -1 is discarded,
and is often used when defining multiple macros, as the remaining newline
characters are typically not wanted in the output.

```m4
divnum
```
`divnum` pushes the active diversion number into the input.

```m4
dnl
```
`dnl` deletes up to (and including) the next newline character. Often used
for single-line *comments* that you do not want to see in the output, or for
removing the newline character after a macro definition.

```m4
dumpdef[(macro_name[, ... ])]
```
`dumpdef` prints the definition of the macros specified as arguments
(which should be quoted) to `stderr`. Useful as a help command, as it gives
the usage syntax for built-in macros. It lists all macros when called without
arguments, which is very useful when debugging.

```m4
errexit
```
This is an extension to the POSIX standard. It causes m4 to exit upon the first
user-related error.

```m4
errok
```
This is an extension to the POSIX standard. It causes m4 to continue execution
even when user-related errors occur. This is the default mode, and the expected
behaviour under the POSIX standard.

```m4
warnerr
```
This is an extension to the POSIX standard. `warnerr` treats warnings as errors,
which will then be affected by `errexit` and `errok`.

```m4
warnok
```
This is an extension to the POSIX standard. `warnok` makes warnings not to be
teated as errors. This is the default mode, and the expected behaviour under the
POSIX standard.

```m4
traceon[(macro_name[, ... ])]
```
Prints to `stderr` the location in the input file, the name of macro, and the
macro stack depth after they are invoked. This is at the beginning of argument
collection, not at the end when the macro is executed. The benefit in tracing
the macros this way, is that they appear in the same order as they do in the
source code, making debugging easier.

When called without arguments, then all of the existing macros are added to
the trace list (which is implemented as a separate hash table). When called
with arguments, then those specified macro names are added to the trace
list. Please note that to add a name to the trace list, the name must
be a valid macro name, but the macro need not exist. The list operates purely
on the text of the macro name, and hence, renaming macros does not inherit
the tracing status.

New macro names created after `traceon` called without arguments are not
automatically added, but `traceon` can be called again to add them.

```m4
traceoff[(macro_name[, ... ])]
```
When called without arguments, `traceoff`, clears the trace list and turns off
the tracing mechanism. When called with arguments, if tracing is on, then the
specified names are removed from the trace list (if tracing is off, then there
is no need to remove them as the list is cleared anyway).

```m4
errprint(error_message)
```
`errprint` prints a message to `stderr`.

```m4
syscmd(shell_command)
```
`syscmd` runs an operating system specific shell command. Nothing is returned
(pushed back into the input). No redirection of standard streams is performed.

```m4
esyscmd(shell_command)
```
`esyscmd` runs an operating system specific shell command and reads the
`stdout` of that command into the input.

```m4
eval(arithmetic_expression[, base, pad, verbose])
```
`eval` evaluates an arithmetic expression.
Works with signed _long_ integers. The default base is 10, but the `base` used
to display the result can be specified.
`pad` adds leading zeros to display the result with a minimum width.
If verbose is 1, then the postfix form of the expression is
printed to `stderr`.

`eval` interprets numbers starting with `0x` as hexadecimal and numbers
commencing with `0` as octal. For example:

```m4
eval(0xF - 010)
7
eval(0xBEEF + 0xCAFE)
100845
m4exit
```

The table below lists the operators (and parentheses) that are understood
by `eval`, along with their properties.

| Operator | Description | Precedence | Number of operands | Associativity |
| :------- | :---------- | ---------: | -----------------: | :-----------: |
| `(`      | Left parenthesis      | 12 | 0 |   N/A |
| `)`      | Right parenthesis     | 12 | 0 |   N/A |
| `+ve`    | Positive              | 11 | 1 | Right |
| `-ve`    | Negative              | 11 | 1 | Right |
| `~`      | Bitwise complement    | 11 | 1 | Right |
| `!`      | Logical negation      | 11 | 1 | Right |
| `**`     | Exponentiation        | 10 | 2 | Right |
| `*`      | Multiplication        |  9 | 2 |  Left |
| `/`      | Division              |  9 | 2 |  Left |
| `%`      | Modulo                |  9 | 2 |  Left |
| `+`      | Addition              |  8 | 2 |  Left |
| `-`      | Subtraction           |  8 | 2 |  Left |
| `<<`     | Bitwise left shift    |  7 | 2 |  Left |
| `>>`     | Bitwise right shift   |  7 | 2 |  Left |
| `<`      | Less than             |  6 | 2 |  Left |
| `<=`     | Less than or equal    |  6 | 2 |  Left |
| `>`      | Greater than          |  6 | 2 |  Left |
| `>=`     | Greater than or equal |  6 | 2 |  Left |
| `==`     | Equal                 |  5 | 2 |  Left |
| `!=`     | Not equal             |  5 | 2 |  Left |
| `&`      | Bitwise AND           |  4 | 2 |  Left |
| `^`      | Bitwise XOR           |  3 | 2 |  Left |
| `\|`     | Bitwise OR            |  2 | 2 |  Left |
| `&&`     | Logical AND           |  1 | 2 |  Left |
| `\|\|`   | Logical OR            |  0 | 2 |  Left |


```m4
ifdef(macro_name, when_defined[, when_undefined])
```
`ifdef` checks to see if the first argument is a macro, and if so, pushes the
second argument back into the input. Otherwise, the third argument (if present)
is pushed back into the input. The macro name should be quoted to prevent it
from expanding during argument collection. Also, importantly, macros will be
expanded and processed immediately during argument collection, _before_ the
branch in logic. So, the second and third arguments should also be quoted.

For example, in the code below, `x` is defined, so we are expecting the result
of `great`. However, surprisingly, `y` is defined as `10` during argument
collection (as quotes were not used), even through this was not the logical
branch taken at the final execution of the macro.

```m4
define(x, cool)

ifdef(`x', great, define(y, 10))
great
y
10
m4exit
```

```m4
ifelse(switch, case_a, when_a[, case_b, when_b, ... ][, default])
```
`ifelse` is like a switch statement in C. The first argument is the str
that is compared against the 2, 4, 6, ... arguments, and upon the first match
the next argument is pushed back into the input. Finally, if there is no
match, then the default argument (the last argument, if present) is pushed back
into the input.

Remember that arguments will be expanded and processed during argument
collection, which occurs _before_ the branch in logic. So, it is a good idea
to quote arguments; 3, 5, 7, ... and the last argument.

```m4
shift(arg1[, ... ])
```
`shift` returns (pushes into the input) a comma-separated list of individually
quoted arguments, excluding the first argument.

```m4
include(filename)
```
`include` pushes the contents of a file into the input. Macros will be
processed.

```m4
sinclude(filename)
```
`sinclude` is a *silent* version of `include`, that does not generate an error
or warning if the file cannot be opened.

```m4
incr(number)
```
`incr` increments a number. The result is pushed into the input.

```m4
decr(number)
```
`decr` decrements a number. The result is pushed into the input.

```m4
len(str)
```
`len` pushes the string length of its first argument into the input.

```m4
index(big_str, small_str)
```
`index` returns the starting offset of where `small_str` is found inside
`big_str`. Offsets commence from zero. If there is no match, then -1
is returned (pushed into the input).

```m4
substr(str, start_index[, size])
```
`substr` returns (pushes to the input) a portion of `str` commencing from
`start_index` and continuing for `size` characters, or until the end of `str`,
if `size` is not specified. Indices commence from zero.

```m4
translit(str, from_chars, to_chars)
```
`translit` performs character-wise replace on `str` and pushes the result into
the input. A mapping of `from_chars` to `to_chars` is internally created in
order to perform the replacement. Each specified character in `from_chars`
is swapped to the corresponding character in `to_chars`. If `from_chars`
is longer than `to_chars`, then the characters without partners will be
deleted.

If the same character appears multiple times in the `from_chars`, then the
first appearance takes precedence (permitted, but unspecified by the POSIX
standard).

Ranges can be specified in the `from_chars` and the `to_chars` by
using a `-` between two characters. These ranges are logically expanded before
the mapping alignment is performed, meaning that the ranges do not need to be
of the same size. Ranges can also be descending, that is the starting
character can have a higher ASCII value than the ending character.
The start and end characters are included in the range.
Ranges are permitted, but unspecified by the POSIX standard.

```m4
m4wrap(code_to_include_at_end)
```
`m4wrap` stores code to be pushed into the input when `EOF` is reached
(before the diversions are automatically undiverted).
This code will then be evaluated as normal. Code will be evaluated in
chronological order if `m4wrap` was called multiple times. This is
useful for performing clean up.

```m4
lsdir[(dir_name)]
```
This is an extension to the POSIX standard. `lsdir` inserts a directory
listing, with a line of hyphens separating the directories (shown first)
from the files (shown second). If no argument is supplied, then the
current working directory is used.

```m4
m4exit[(exit_value)]
```
`m4exit` allows the user to request early termination of m4, specifying the
_desired_ exit value in the first argument. The requested value must be between
zero and `UCHAR_MAX`, inclusive. If called without arguments, then zero is the
default value. Please note that a requested return value of zero will be
overwritten if any errors occurred at any time during the operation of m4.
However, if the macro is called successfully, then a non-zero requested return
value will be used as the final return value of m4, regardless of other errors.

`m4exit` causes immediate termination, `m4wrap` is not performed and the
diversions are not undiverted.

```m4
recrm(file_path)
```
This is an extension to the POSIX standard. `recrm` recursively removes a path
if it exists. Any sub-file or sub-directories will be deleted along with the
specified path itself.

```m4
regexrep(text, regex_find, replace[, newline_insensitive, verbose])
```
This is an extension to the POSIX standard. `regexrep` searches text for
a regex pattern and replaces the matches. If the fourth argument is 1,
then newline insensitive matching occurs.
If verbose is 1, then the posfix form of the expression and the
nondeterministic finite automaton (NFA) structure are printed to `stderr`.

```m4
sysval
```
`sysval` pushes the return value of the last shell command run via `syscmd`
or `esyscmd` into the input.

```m4
tnl(str)
```
This is an extension to the POSIX standard. `tnl` trims trailing newline
characters from the end of the first argument.
This is useful in conjunction with `esyscmd` as trailing newline characters
are not stripped, as users are normally accustomed to with POSIX sh command
substitution.

For example, in the code below, `tnl` eats the trailing newline from the shell
command, preventing the sentence from being broken.

```m4
My name is esyscmd(whoami), hello!
My name is logan
, hello!
My name is tnl(esyscmd(whoami)), hello!
My name is logan, hello!
m4exit
```

```m4
undefine(macro_name)
```
`undefine` removes a macro and all of it history stack. Normally you should
quote the macro name to prevent it from expanding into its definition during
argument collection. Built-in macros cannot be retrieved once undefined,
unless you have previously made a renamed copy of them.

In the example below, `define` and `x` (via its inheritance) do not need to
be quoted when "called" without arguments, as they exhibit pass-through
behaviour (only certain built-in macro do this). The example clones
`define` to `x`, deletes `define`, then restores it from `x`.

```m4
define(x, defn(define))

undefine(define)

dumpdef(define)
Undefined: define

x(define, defn(x))

dumpdef(define)
Built-in: define(macro_name, macro_def)

m4exit
```

```m4
popdef(macro_name)
```
`popdef` removes the current history from the history stack, making the new
macro what the prior history was. If there was no prior history, then `popdef`
has the same effect as `undefine`. You will normally want to quote the macro
name.

```m4
undivert[(div_num_or_filename)]
```
`undivert` appends the contents of a diversion or file onto the current active
diversion. Undiverted diversions are emptied. A diversion cannot be undiverted
into itself, and diversion -1 cannot be undiverted (as it is discarded).
It is important to note that no processing occurs during this, macros are not
expanded. If an argument contains any non-digit characters, then it will be
treated as a filename (this is allowed, but unspecified by the POSIX standard).

```m4
writediv(div_num, filename[, append])
```
This is an extension to the POSIX standard. `writediv` empties the specified
diversion to file. It creates missing directories in the file path.
If append is 1, then it will append to the end of the file, otherwise the file
will be overwritten.

This macro is very useful for writing dynamic code. The code can be crafted
into a diversion, then that diversion can be written to file using `writediv`,
and then the file can be executed using `esyscmd`.

```m4
maketemp(templateXXXXXX)
```
`maketemp` replaces the trailing X's in `templateXXXXXX` with the pid
of the current process, and pushes the result back into the input.
It does not check if a file with that name already exists, and it
does not create a file. It is depreciated and should not be used.

```m4
mkstemp(templateXXXXXX)
```
`mkstemp` replaces the trailing X's in `templateXXXXXX` with random
characters and creates and closes a file by that name, pushing the
resultant filename into the input. Where available, this is done by
calling the C function `mkstemp` from `<stdlib.h>`.


bc
==

bc is a cross-platform basic calculator. It reads from `stdin` and works with
signed _long_ integers. See the m4 built-in macro `eval` above for more
details.


freq
====

`freq` determines the character frequency in a file. Non-graph characters are
displayed using their hex value. A character and its count are separated by
a space, and only characters present in the file are reported.

Usage:
```
freq file
```

toco_regex
==========

toco_regex is the built-in regular expression engine.

Special escape sequences are processed first. This occurs on the find and the
replace strings as a preprocessing step (before any regex content is
interpreted). These consist of a subset of the C escape sequences, most notably
the octal escape sequences are omitted. The recognised escape sequences are:

* `\0`
* `\a`
* `\b`
* `\t`
* `\n`
* `\v`
* `\f`
* `\r`
* `\xBE`

Where `BE` can be any two hexadecimal digits.


Enjoy,
Logan =)_
