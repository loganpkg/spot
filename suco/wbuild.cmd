::
:: Copyright (c) 2025, 2026 Logan Ryan McLintock. All rights reserved.
::
:: Redistribution and use in source and binary forms, with or without
:: modification, are permitted provided that the following conditions
:: are met:
:: 1. Redistributions of source code must retain the above copyright
::    notice, this list of conditions and the following disclaimer.
:: 2. Redistributions in binary form must reproduce the above copyright
::    notice, this list of conditions and the following disclaimer in the
::    documentation and/or other materials provided with the distribution.
::
:: THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND
:: ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
:: IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
:: ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
:: FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
:: DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
:: OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
:: HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
:: LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
:: OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
:: SUCH DAMAGE.
::

:: ###################################################################
::  Configuration:
set suco_backup_dir=%HOMEDRIVE%%HOMEPATH%\OneDrive\Documents\SUCO_BACKUP
set install_dir=%HOMEDRIVE%%HOMEPATH%\bin
set c_ops=/Qspectre /Wall /wd4820 /wd5045 /I. /DDEBUG
:: ###################################################################

:: Escape backslashes for C string.
set suco_backup_dir=%suco_backup_dir:\=\\%

set c_ops=%c_ops% -DSUCO_BACKUP_DIR=%suco_backup_dir%

cl /c %c_ops% *.c .\test\*.c

cl %c_ops% test_buf.obj buf.obj int.obj /Fe.\test\test_buf.exe

cl %c_ops% test_input.obj input.obj buf.obj int.obj ^
    /Fe.\test\test_input.exe

cl %c_ops% test_screen.obj screen.obj int.obj ^
    /Fe.\test\test_screen.exe

cl %c_ops% test_gap_buf.obj gap_buf.obj memmem.obj screen.obj input.obj ^
    buf.obj int.obj /Fe.\test\test_gap_buf.exe

cl %c_ops% test_dll.obj doubly_linked_list.obj ^
    /Fe.\test\test_dll.exe

cl %c_ops% suco.obj gap_buf.obj memmem.obj screen.obj input.obj buf.obj ^
   int.obj get_time.obj asprintf.obj random.obj /Fesuco.exe /link bcrypt.lib

del *.obj

:: Install.
if not exist %install_dir% mkdir %install_dir%
copy suco.exe %install_dir%
