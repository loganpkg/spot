divert(-1)

#
# Copyright (c) 2023, 2024 Logan Ryan McLintock. All rights reserved.
#
# The contents of this file are subject to the
# Common Development and Distribution License (CDDL) version 1.1
# (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License in
# the LICENSE file included with this software or at
# https://opensource.org/license/cddl-1-1
#
# NOTICE PURSUANT TO SECTION 4.2 OF THE LICENSE:
# This software is prohibited from being distributed or otherwise made
# available under any subsequent version of the License.
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
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
