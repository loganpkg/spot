/*
 * Copyright (c) 2023 Logan Ryan McLintock
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * spot: text editor.
 * Dedicated to my son who was only a 4mm "spot" in his first ultrasound.
 *
 * Jesus answered, "Those who drink this water will get thirsty again,
 * but those who drink the water that I will give them will never be thirsty
 * again. The water that I will give them will become in them a spring which
 * will provide them with life-giving water and give them eternal life."
 *                                                          John 4:13-14 GNT
 */


#include "toucanlib.h"


#define INIT_GB_SIZE 512


int draw(struct gb *b, struct gb *cl, struct screen *s, int cl_active,
         int rv, int es_set, int es)
{
    size_t up, target_up, ts, i, cursor_pos, k;
    int have_centred = 0;
    unsigned char ch, *sb;
    char num_str[NUM_BUF_SIZE];
    int r, len;

  start:
    if (erase_screen(s))
        return ERR;

    if (b->d > b->g || s->centre) {
        /* Does not consider long lines that wrap */
        b->d = b->g;
        up = 0;
        target_up = s->h <= 4 ? 1 : (s->h - 1) / 2;
        while (b->d) {
            --b->d;
            if (*(b->a + b->d) == '\n')
                ++up;

            if (up == target_up) {
                ++b->d;
                break;
            }
        }
        s->centre = 0;
        have_centred = 1;
    }

    /* Size of the text portion of the screen */
    ts = s->vs_s - (s->h >= 2 ? s->w : 0) - (s->h >= 3 ? s->w : 0);

    s->v_hl = 0;

    /* Region commenced before draw start */
    if (b->m_set && b->m < b->d)
        s->v_hl = 1;

    /* Before the gap */
    i = b->d;
    while (i < b->g && s->v_i < ts) {
        if (b->m_set && b->m == i)
            s->v_hl = 1;

        ch = *((b)->a + i);
        print_ch(s, ch);
        ++i;
    }

    if (s->v_i == ts) {
        /* Off screen */
        if (!have_centred)
            s->centre = 1;
        else
            b->d = b->g;
        goto start;
    }

    /* At the cursor */
    cursor_pos = s->v_i;
    if (b->m_set) {
        if (b->m < b->c)
            s->v_hl = 0;        /* End of region */
        else
            s->v_hl = 1;        /* Start of region */
    }

    /* After the gap (from the cursor onwards) */
    i = b->c;
    while (i <= b->e && s->v_i < ts) {
        if (b->m_set && b->m == i)
            s->v_hl = 0;

        ch = *((b)->a + i);
        print_ch(s, ch);
        ++i;
    }

    if (s->h >= 2) {
        /* Status bar */

        if (es_set) {
            r = snprintf(num_str, NUM_BUF_SIZE, "%d", es);
            if (r < 0 || r >= NUM_BUF_SIZE)
                return ERR;
        } else {
            *num_str = '\0';
        }

        sb = s->vs_n + ((s->h - 2) * s->w);
        len = snprintf((char *) sb, s->w, "%c%c %s (%lu,%lu) %02X %s",
                       rv ? '!' : ' ', b->mod ? '*' : ' ', b->fn,
                       (unsigned long) b->r, (unsigned long) b->col,
                       cl_active ? *(cl->a + cl->c) : *(b->a + b->c),
                       num_str);
        if (len < 0)
            return ERR;

        /* Overwrite \0 char */
        if ((size_t) len >= s->w)       /* Truncated */
            *(sb + s->w - 1) = ' ';
        else
            *(sb + len) = ' ';

        /* Virtually highlight */
        for (k = 0; k < s->w; ++k)
            *(sb + k) |= '\x80';
    }

    if (s->h >= 3) {
        /* Command line */
        if (cl->d > cl->g || cl->g - cl->d > cl->col
            || cl->g - cl->d >= s->w) {
            if (cl->col < s->w)
                cl->d = cl->g - cl->col;
            else
                cl->d = cl->g;
        }

        s->v_hl = 0;

        if (cl->m_set && cl->m < cl->d)
            s->v_hl = 1;

        /* Start of last line in virtual screen */
        s->v_i = (s->h - 1) * s->w;

        /* Before the gap */
        i = cl->d;
        while (i < cl->g && s->v_i < s->vs_s) {
            if (cl->m_set && cl->m == i)
                s->v_hl = 1;

            ch = *((cl)->a + i);
            print_ch(s, ch);
            ++i;
        }

        /* At the cursor */
        if (cl_active)
            cursor_pos = s->v_i;

        if (cl->m_set) {
            if (cl->m < cl->c)
                s->v_hl = 0;    /* End of region */
            else
                s->v_hl = 1;    /* Start of region */
        }

        /* After the gap (from the cursor onwards) */
        i = cl->c;
        while (i <= cl->e && s->v_i < s->vs_s) {
            if (cl->m_set && cl->m == i)
                s->v_hl = 0;

            ch = *((cl)->a + i);
            print_ch(s, ch);
            ++i;
        }
    }

    s->v_i = cursor_pos;
    refresh_screen(s);

    return 0;
}


#define z (cl_active ? cl : b)

int main(int argc, char **argv)
{
    int ret = 0;                /* Return value of the text editor */
    int running = 1;            /* Indicates if the text editor is running */
    int rv = 0;                 /* Return value of last internal command */
    int es_set = 0;
    int es = 0;                 /* Exit status of last shell command */
    int i, x, y;
    struct screen *s = NULL;
    struct gb *b = NULL;        /* Text buffers linked together */
    struct gb *p = NULL;        /* Paste buffer */
    struct gb *cl = NULL;       /* Command line buffer */
    struct gb *sb = NULL;       /* Search buffer */
    struct gb *tmp = NULL;      /* Temporary buffer */
    struct gb *t;               /* For switching gap buffers */
    char search_type = ' ';     /* s = Exact search, z = Regex search */
    int cl_active = 0;          /* Cursor is in the command line */
    char op = ' ';              /* The cl operation which is in progress */


    if ((s = init_screen()) == NULL)
        return ERR;


    if (argc > 1) {
        for (i = 1; i < argc; ++i) {
            if (new_gb(&b, *(argv + i), INIT_GB_SIZE))
                goto clean_up;
        }
        while (b->prev)
            b = b->prev;
    } else {
        /* No args */
        if (new_gb(&b, NULL, INIT_GB_SIZE))
            goto clean_up;
    }

    if ((p = init_gb(INIT_GB_SIZE)) == NULL)
        goto clean_up;

    if ((cl = init_gb(INIT_GB_SIZE)) == NULL)
        goto clean_up;

    if ((sb = init_gb(INIT_GB_SIZE)) == NULL)
        goto clean_up;

    if ((tmp = init_gb(INIT_GB_SIZE)) == NULL)
        goto clean_up;

    while (running) {
        if (draw(b, cl, s, cl_active, rv, es_set, es))
            goto clean_up;

        rv = 0;
        es_set = 0;
        es = 0;

        x = get_key();

        switch (x) {
        case C('b'):
        case LEFT_KEY:
            rv = left_ch(z);
            break;
        case C('f'):
        case RIGHT_KEY:
            rv = right_ch(z);
            break;
        case C('p'):
        case UP_KEY:
            rv = up_line(z);
            break;
        case C('n'):
        case DOWN_KEY:
            rv = down_line(z);
            break;
        case C('d'):
        case DEL_KEY:
            rv = delete_ch(z);
            break;
        case C('h'):
        case 127:
            rv = backspace_ch(z);
            break;
        case C('a'):
        case HOME_KEY:
            start_of_line(z);
            break;
        case C('e'):
        case END_KEY:
            end_of_line(z);
            break;
        case 0:
            z->m_set = 1;
            z->m = z->c;
            break;
        case C('g'):
            if (z->m_set) {
                z->m_set = 0;
                z->m = 0;
            } else if (cl_active) {
                delete_gb(cl);
                cl_active = 0;
            }
            break;
        case C('l'):
            s->centre = 1;
            s->clear = 1;
            break;
        case C('w'):
            rv = copy_region(z, p, 1);
            break;
        case C('y'):
            rv = paste(z, p);
            break;
        case C('k'):
            rv = cut_to_eol(z, p);
            break;
        case C('t'):
            trim_clean(z);
            break;
        case C('s'):
            delete_gb(cl);
            cl_active = 1;
            op = 's';
            break;
        case C('z'):
            delete_gb(cl);
            cl_active = 1;
            op = 'z';
            break;
        case C('r'):
            delete_gb(cl);
            cl_active = 1;
            op = 'r';
            break;
        case C('u'):
            delete_gb(cl);
            cl_active = 1;
            op = 'u';
            break;
        case C('q'):
            delete_gb(cl);
            cl_active = 1;
            op = 'q';
            break;
        case ESC:
            y = get_key();
            switch (y) {
            case 'b':
                left_word(z);
                break;
            case 'f':
                right_word(z, ' ');
                break;
            case 'l':
                /* Lowercase word */
                right_word(z, 'L');
                break;
            case 'u':
                /* Uppercase word */
                right_word(z, 'U');
                break;
            case 'k':
                rv = cut_to_sol(z, p);
                break;
            case 'm':
                rv = match_bracket(z);
                break;
            case 'n':
                /* Repeat last search */
                if (search_type == 's')
                    rv = exact_forward_search(b, sb);
                else if (search_type == 'z')
                    rv = regex_forward_search(b, sb);
                else
                    rv = 1;

                break;
            case 'w':
                rv = copy_region(z, p, 0);
                break;
            case '!':
                remove_gb(&b);
                if (b == NULL)
                    running = 0;

                break;
            case '=':
                delete_gb(cl);
                if (b->fn != NULL && insert_str(cl, b->fn))
                    delete_gb(cl);

                cl_active = 1;
                op = '=';
                break;
            case '$':
                delete_gb(cl);
                cl_active = 1;
                op = '$';       /* insert_shell_cmd */
                break;
            case '`':
                rv = shell_line(z, tmp, &es);
                if (!rv)
                    es_set = 1;

                break;
            case '<':
                start_of_gb(z);
                break;
            case '>':
                end_of_gb(z);
                break;
            }
            break;
        case C('x'):
            y = get_key();
            switch (y) {
            case C('x'):
                rv = swap_cursor_and_mark(z);
                break;
            case C('c'):
                running = 0;
                break;
            case C('s'):
                rv = save(b);   /* Cannot save the command line */
                break;
            case C('f'):
                delete_gb(cl);
                cl_active = 1;
                op = 'f';
                break;
            case 'i':
                delete_gb(cl);
                cl_active = 1;
                op = 'i';
                break;
            case LEFT_KEY:
                if (b->prev != NULL)
                    b = b->prev;

                break;
            case RIGHT_KEY:
                if (b->next != NULL)
                    b = b->next;

                break;
            }
            break;
        case '\r':
        case '\n':
            if (cl_active) {
                switch (op) {
                case 's':
                    /* Swap gap buffers */
                    t = sb;
                    sb = cl;
                    cl = t;
                    delete_gb(cl);
                    search_type = op;
                    rv = exact_forward_search(b, sb);
                    break;
                case 'z':
                    /* Swap gap buffers */
                    t = sb;
                    sb = cl;
                    cl = t;
                    delete_gb(cl);
                    search_type = op;
                    rv = regex_forward_search(b, sb);
                    break;
                case 'r':
                    rv = regex_replace_region(b, cl);
                    break;
                case '=':
                    start_of_gb(cl);
                    rv = rename_gb(b, (const char *) cl->a + cl->c);
                    break;
                case 'u':
                    rv = goto_row(b, cl);
                    break;
                case 'q':
                    rv = insert_hex(b, cl);
                    break;
                case 'f':
                    start_of_gb(cl);
                    rv = new_gb(&b, (const char *) cl->a + cl->c,
                                INIT_GB_SIZE);
                    break;
                case 'i':
                    start_of_gb(cl);
                    rv = insert_file(b, (const char *) cl->a + cl->c);
                    break;
                case '$':
                    start_of_gb(cl);
                    rv = insert_shell_cmd(b, (const char *) cl->a + cl->c,
                                          &es);
                    if (!rv)
                        es_set = 1;

                    break;
                }
                cl_active = 0;
                op = ' ';
            } else {
                rv = insert_ch(z, '\n');
            }
            break;
        default:
            if (isprint(x) || x == '\t')
                rv = insert_ch(z, x);

            break;
        }
    }


  clean_up:
    if (free_screen(s))
        ret = ERR;

    free_gb_list(b);
    free_gb(p);
    free_gb(cl);
    free_gb(sb);
    free_gb(tmp);

    return ret;
}

#undef z
