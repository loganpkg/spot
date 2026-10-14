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

#include <stdio.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <alias.h>
#include <input.h>

int main(void)
{
    Input ip;
    int r, ch;
    int non_blocking = 0;

    struct key_map km[] = {
        { { CTRL_A, CTRL_B, CTRL_C, CTRL_D, CTRL_E, CTRL_F }, 0x200 },
        { { ESC, 'x' }, 0x201 },
        { { KEY_F1, KEY_F2, KEY_F12, KEY_DELETE, KEY_PAGE_UP }, 0x202 },
        { { 0 }, 0 },
    };

    if (init_input_stdin(&ip, BLOCKING, RAW, NULL))
        return 1;

    while (1) {
        printf("=> ");

        if (fflush(stdout)) {
            free_input(ip);
            return 1;
        }

        if (non_blocking)
            sleep(2);

        r = get_ch(ip, &ch);
        if (r == 1) {
            free_input(ip);
            return 1;
        }

        if (ch == EOF)
            break;

        if (r == WOULD_BLOCK)
            printf("_ \r\n");
        else
            printf("%02X \r\n", ch);

        if (fflush(stdout)) {
            free_input(ip);
            return 1;
        }

        if (ch == 'q')
            break;
        else if (ch == 'n') {
            if (free_input(ip))
                return 1;

            if (init_input_stdin(&ip, NON_BLOCKING_TTY, COOKED, NULL))
                return 1;

            non_blocking = 1;
        } else if (ch == 'd') {
            if (free_input(ip))
                return 1;

            if (init_input_stdin(&ip, NON_BLOCKING_TTY, DOUBLE_COOKED, km))
                return 1;

            non_blocking = 1;
        }
    }

    if (free_input(ip))
        return 1;

    return 0;
}
