divert(-1)

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
