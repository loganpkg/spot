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

#ifndef SCREEN_H
#define SCREEN_H

#include <stddef.h>

/* The size that these characters consume when displayed on the screen. */
#define TAB_SIZE     8
#define CTRL_CH_SIZE 2

#define HARD_CLEAR 1
#define SOFT_CLEAR 2

typedef struct screen *Screen;

extern int dummy;

/* Function declarations */
int clear_screen(Screen sc, int mode);

int free_screen(Screen sc);

Screen init_screen(void);

int soft_clear_sub_screen(
    Screen sc, size_t y_origin, size_t x_origin, size_t sub_h, size_t sub_w);

int sub_screen_print_ch(Screen sc, size_t y_origin, size_t x_origin,
    size_t sub_h, size_t sub_w, char ch);

int sub_screen_print_str(Screen sc, size_t y_origin, size_t x_origin,
    size_t sub_h, size_t sub_w, const char *str);

int print_ch(Screen sc, char ch);

int print_str(Screen sc, const char *str);

int refresh_screen(Screen sc);

int move(Screen sc, size_t y, size_t x);

size_t get_screen_height(Screen sc);

size_t get_screen_width(Screen sc);

size_t get_y(Screen sc);

size_t get_x(Screen sc);

void highlight_on(Screen sc);

void highlight_off(Screen sc);

#endif
