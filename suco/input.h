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

#ifndef INPUT_H
#define INPUT_H

/* blocking: */
#define BLOCKING         1
#define NON_BLOCKING_TTY 2

/* cooking: */
/* No change to the raw characters.*/
#define RAW 4

/* Converts inconsistent or multi-byte keys into ints. */
#define COOKED 8

/* Maps sequences of keys to ints using user-defined mapping. */
#define DOUBLE_COOKED 16

/* Return value: */
#define WOULD_BLOCK 2

/* Escape: */
#define ESC 0x1B

/* Control characters: */
#define CTRL_2 0x00
#define CTRL_A 0x01
#define CTRL_B 0x02
#define CTRL_C 0x03
#define CTRL_D 0x04
#define CTRL_E 0x05
#define CTRL_F 0x06
#define CTRL_G 0x07
#define CTRL_H 0x08
#define CTRL_I 0x09
#define CTRL_J 0x0A
#define CTRL_K 0x0B
#define CTRL_L 0x0C
#define CTRL_M 0x0D
#define CTRL_N 0x0E
#define CTRL_O 0x0F
#define CTRL_P 0x10
#define CTRL_Q 0x11
#define CTRL_R 0x12
#define CTRL_S 0x13
#define CTRL_T 0x14
#define CTRL_U 0x15
#define CTRL_V 0x16
#define CTRL_W 0x17
#define CTRL_X 0x18
#define CTRL_Y 0x19
#define CTRL_Z 0x1A

/* Cooked keyboard keys: */
#define KEY_BACKSPACE 0x0100
#define KEY_LEFT      0x0101
#define KEY_RIGHT     0x0102
#define KEY_UP        0x0103
#define KEY_DOWN      0x0104
#define KEY_HOME      0x0105
#define KEY_END       0x0106
#define KEY_INSERT    0x0107
#define KEY_DELETE    0x0108
#define KEY_PAGE_UP   0x0109
#define KEY_PAGE_DOWN 0x010A
#define KEY_F1        0x010B
#define KEY_F2        0x010C
#define KEY_F3        0x010D
#define KEY_F4        0x010E
#define KEY_F5        0x010F
#define KEY_F6        0x0110
#define KEY_F7        0x0111
#define KEY_F8        0x0112
#define KEY_F9        0x0113
#define KEY_F10       0x0114
#define KEY_F11       0x0115
#define KEY_F12       0x0116
#define CTRL_LEFT     0x0117
#define CTRL_RIGHT    0x0118
#define CTRL_UP       0x0119
#define CTRL_DOWN     0x011A

#define MAX_SEQ 10

struct key_map {
    /* 0 in an element, besides the first, terminates the sequence early. */
    int seq[MAX_SEQ];
    /* 0 terminates an array of this struct. */
    int key;
};

typedef struct input *Input;

/* Function declarations */
int free_input(Input ip);

int init_input_stdin(Input *ip, int blocking, int cooking,
    const struct key_map *second_level_km);

int init_input_fn(Input *ip, const char *fn, int blocking, int cooking,
    const struct key_map *second_level_km);

int get_ch(Input ip, int *ch);

int unget_ch(Input ip, int ch);

#endif
