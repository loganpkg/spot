/*
 * Copyright (c) 2025 Logan Ryan McLintock. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <stdio.h>

#include <alias.h>
#include <debug.h>
#include <screen.h>

#define ALPHABET_SIZE 26

int print_alphabet(Screen sc, int mode, int increment)
{
    unsigned char u;
    size_t i, j;

    u = 'A';
    for (i = 0; i < ALPHABET_SIZE; ++i) {
        if (clear_screen(sc, mode))
            debug(return 1);

        for (j = 0; j <= i; j++) print_ch(sc, u);

        if (refresh_screen(sc))
            debug(return 1);

        if (increment)
            ++u;

        sleep(1);
    }
    return 0;
}

int main(void)
{
    Screen sc;

    if ((sc = init_screen()) == NULL)
        debug(return 1);

    sleep(2);

    if (print_alphabet(sc, HARD_CLEAR, 0))
        debug(goto error);

    if (print_alphabet(sc, SOFT_CLEAR, 0))
        debug(goto error);

    if (print_alphabet(sc, HARD_CLEAR, 1))
        debug(goto error);

    if (print_alphabet(sc, SOFT_CLEAR, 1))
        debug(goto error);

    highlight_on(sc);

    if (print_str(sc, "cool world\n"))
        debug(goto error);

    highlight_off(sc);

    if (print_str(sc, "\x01\x1B\n"))
        debug(goto error);

    if (move(sc, 0, 4))
        debug(goto error);

    if (refresh_screen(sc))
        debug(goto error);

    sleep(1);

    while (!print_str(sc, "\x05\xFF\telephant"))
        if (refresh_screen(sc))
            debug(goto error);

    sleep(1);

    return free_screen(sc);

error:
    free_screen(sc);
    debug(return 1);
}
