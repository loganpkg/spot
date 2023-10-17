divert(-1)
cmp <(m4 test.m4) <(/usr/bin/m4 test.m4)
divert
define(x, y)
x
undefine(`x')
x
define(x, $1 and $2)
x(wow, cool)
x(x(a, b), x(c, d))
x(a, (b, c))
define
divert(8)
divnum
cool
divert(2)
divnum
hello
divert(0)
undivert
divert(9)
divnum
elephant
divert(3)
divnum
banana
divert(2)
divnum
undivert(9, 3)
divert
divnum
undivert(2)
changequote([, ])
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
changequote
esyscmd(echo `hello' > .t)
include(.t)
esyscmd(rm .t)
