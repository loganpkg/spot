divert(-1)

#
# Copyright (c) 2023 Logan Ryan McLintock
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

changequote([, ])
divert(0)
define(x, y)
x
undefine([x])
x
define(x, $1 and $2)
x(wow, cool)
x(x(a, b), x(c, d))
x(a, (b, c))
[define]
divert(8)
divnum
cool
divert(2)
divnum
hello
divert(0)
undivert(2)
undivert(8)
divert(9)
divnum
elephant
divert(3)
divnum
banana
divert(2)
divnum
undivert(9)
undivert(3)
divert(0)
divnum
undivert(2)
define([x], cool)
ifdef([x], dog, cat)
ifelse(x, cool, nice, wow)
esyscmd(echo llama)
sysval
define([x], 0)
define([x], incr(x))
x
define([x], incr(incr(incr(incr(incr(incr(incr(incr(incr(incr(incr(incr(incr(
incr(incr(incr(incr(incr(incr(incr(incr(incr(incr(incr(incr(incr(incr(incr(
incr(x))))))))))))))))))))))))))))))
x
define(
    [x],

    cool $1$2$3)
x(a, b, c)
cool dnl world
define(hello, wow)
changequote({, })
esyscmd(echo {hello} > .t)
include(.t)
esyscmd(rm .t)
changequote(`, ')
define(`x', 10)
define(`y', 20)
define(`z', 30)
define(`w', 40)
pushdef(`z', a)
pushdef(`z', b)
pushdef(`z', c)
pushdef(`w', jjjjj)
pushdef(`w', sjsj)
popdef(`z')
popdef(`y')
