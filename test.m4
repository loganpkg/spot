divert(-1)

#
# Copyright (c) 2023, 2024 Logan Ryan McLintock. All rights reserved.
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
