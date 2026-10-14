/*
 * Copyright (c) 2026 Logan Ryan McLintock. All rights reserved.
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

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "write_to_window.h"

#define mult_overflow(a, b) ((b) && (a) > SIZE_MAX / (b))
#define add_overflow(a, b)  ((a) > SIZE_MAX - (b))

static BOOL CALLBACK print_window_name(HWND h, LPARAM par)
{
    int len;
    char *nm = NULL;

    len = GetWindowTextLengthA(h);

    if (!len) {
        if (GetLastError())
            debug(return FALSE);
        else
            return TRUE;
    }

    if (len == INT_MAX)
        debug(return FALSE);

    if ((nm = malloc(len + 1)) == NULL)
        debug(return FALSE);

    if (!GetWindowTextA(h, nm, len + 1))
        debug(return FALSE);

    printf("%s\n", nm);

    free(nm);

    return TRUE;
}

int print_all_ascii_as_keys(void)
{
    unsigned char u, v_key, shift_state;
    short y;
    HKL us_kb;

    if ((us_kb = LoadKeyboardLayoutA("00000409", KLF_NOTELLSHELL)) == NULL)
        debug(return 1);

    for (u = 0; u < 0x80; ++u) {
        if ((y = VkKeyScanExA(u, us_kb)) == -1)
            debug(return 1);

        /* Lower byte. */
        v_key = y & 0xff;

        fprintf(stderr, "%02X", u);

        /* Upper byte. */
        shift_state = y >> 8 & 0xff;

        if (shift_state & 1)
            fprintf(stderr, ",SHIFT");

        if (shift_state & 2)
            fprintf(stderr, ",CONTROL");

        if (shift_state & 4)
            fprintf(stderr, ",ALT");

        putc('\n', stderr);
    }

    return 0;
}

static int send_keys(const char *text, size_t text_len)
{
    HKL us_kb;
    short y;
    unsigned char v_key, shift_state;
    INPUT *x = NULL;
    unsigned int j;
    size_t s, i;

    WORD modifiers[] = { VK_SHIFT, VK_CONTROL };

#define MAX_INPUT_ELEMENTS_PER_CH 6

    if (mult_overflow(text_len, MAX_INPUT_ELEMENTS_PER_CH))
        debug(goto error);

    s = text_len * MAX_INPUT_ELEMENTS_PER_CH;

    if (add_overflow(s, ARRAYSIZE(modifiers)))
        debug(goto error);

    s += ARRAYSIZE(modifiers);

    if (s > UINT_MAX)
        debug(goto error);

    if (mult_overflow(s, sizeof(INPUT)))
        debug(goto error);

    if ((x = calloc(s, sizeof(INPUT))) == NULL)
        debug(goto error);

    if ((us_kb = LoadKeyboardLayoutA("00000409", KLF_NOTELLSHELL)) == NULL)
        debug(goto error);

    /*
     * There is no way to detect an active dead key.
     * Release all modifiers that are relevant to ASCII.
     */
    for (j = 0; j < ARRAYSIZE(modifiers); ++j) {
        x[j].type = INPUT_KEYBOARD;
        x[j].ki.wVk = modifiers[j];
        x[j].ki.dwFlags = KEYEVENTF_KEYUP;
    }

    for (i = 0; i < text_len; ++i) {

        if ((y = VkKeyScanExA(text[i], us_kb)) == -1)
            debug(goto error);

        /* Lower byte. */
        v_key = y & 0xff;

        /* Upper byte. */
        shift_state = y >> 8 & 0xff;

        if (shift_state & 1) {
            /* Shift key press. */
            x[j].type = INPUT_KEYBOARD;
            x[j++].ki.wVk = VK_SHIFT;
        }

        if (shift_state & 2) {
            /* Control key press. */
            x[j].type = INPUT_KEYBOARD;
            x[j++].ki.wVk = VK_CONTROL;
        }

        /* Key press. */
        x[j].type = INPUT_KEYBOARD;
        x[j++].ki.wVk = v_key;

        /* Key release. */
        x[j].type = INPUT_KEYBOARD;
        x[j].ki.wVk = v_key;
        x[j++].ki.dwFlags = KEYEVENTF_KEYUP;

        if (shift_state & 2) {
            /* Control key release. */
            x[j].type = INPUT_KEYBOARD;
            x[j].ki.wVk = VK_CONTROL;
            x[j++].ki.dwFlags = KEYEVENTF_KEYUP;
        }

        if (shift_state & 1) {
            /* Shift key release. */
            x[j].type = INPUT_KEYBOARD;
            x[j].ki.wVk = VK_SHIFT;
            x[j++].ki.dwFlags = KEYEVENTF_KEYUP;
        }
    }

    if (SendInput(j, x, sizeof(INPUT)) != j)
        debug(goto error);

    return 0;

error:
    if (x != NULL)
        free(x);

    debug(return 1);
}

int write_to_window(const char *window_name, const char *text, size_t text_len)
{
    HWND cw, nw;

    if ((cw = GetForegroundWindow()) == NULL)
        debug(return 1);

    if ((nw = FindWindowA(NULL, window_name)) == NULL)
        debug(return 1);

    if (!SetForegroundWindow(nw))
        debug(return 1);

    Sleep(100);

    if (send_keys(text, text_len)) {
        SetForegroundWindow(cw);
        debug(return 1);
    }

    Sleep(100);

    if (!SetForegroundWindow(cw))
        debug(return 1);

    return 0;
}
