/*
 * Copyright (c) 2025, 2026 Logan Ryan McLintock. All rights reserved.
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

#ifndef GAP_BUF_H
#define GAP_BUF_H

#include <stddef.h>

#include "screen.h"

#define INCLUDE_STATUS_BAR 1
#define EXCLUDE_STATUS_BAR 2

typedef struct gap_buf *Gap_buf;

/* Function declarations */
void gb_free(Gap_buf gb);

void gb_reset(Gap_buf gb);

Gap_buf gb_init(size_t init_num_elements);

int gb_insert_ch(Gap_buf gb, char ch);

int gb_delete_ch(Gap_buf gb);

int gb_left_ch(Gap_buf gb);

int gb_right_ch(Gap_buf gb);

int gb_undo(Gap_buf gb);

int gb_redo(Gap_buf gb);

void gb_start_of_line(Gap_buf gb);

void gb_end_of_line(Gap_buf gb);

void gb_start_of_buffer(Gap_buf gb);

void gb_end_of_buffer(Gap_buf gb);

int gb_up_line(Gap_buf gb);

int gb_down_line(Gap_buf gb);

int gb_forward_search(Gap_buf gb, Gap_buf search);

int gb_match_brace(Gap_buf gb);

int gb_backspace_ch(Gap_buf gb);

int gb_insert_hex_str(Gap_buf gb, const char *hex_str);

int gb_trim_clean(Gap_buf gb);

int gb_insert_file(Gap_buf gb, const char *fn);

const char *gb_to_str(Gap_buf gb);

int gb_set_fn(Gap_buf gb, const char *fn);

const char *gb_get_fn(Gap_buf gb);

void gb_clear_mod(Gap_buf gb);

int gb_is_mod_set(Gap_buf gb);

int gb_write_file(Gap_buf gb, const char *fn);

void gb_set_mark(Gap_buf gb);

void gb_clear_mark(Gap_buf gb);

int gb_is_mark_set(Gap_buf gb);

int gb_copy_region(Gap_buf gb, Gap_buf paste);

int gb_cut_region(Gap_buf gb, Gap_buf paste);

int gb_cut_to_start_of_line(Gap_buf gb, Gap_buf paste);

int gb_cut_to_end_of_line(Gap_buf gb, Gap_buf paste);

int gb_insert_gb(Gap_buf target, Gap_buf source);

void gb_debug_print(Gap_buf gb);

void gb_request_centring(Gap_buf gb);

int gb_print(Gap_buf gb, Screen sc, size_t y_origin, size_t x_origin,
    size_t sub_h, size_t sub_w, int sb_option, size_t *cursor_y,
    size_t *cursor_x);

#endif
