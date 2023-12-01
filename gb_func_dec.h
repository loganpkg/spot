struct gb *init_gb(size_t s);
void free_gb(struct gb *b);
void free_gb_list(struct gb *b);
void delete_gb(struct gb *b);
int insert_ch(struct gb *b, char ch);
int insert_str(struct gb *b, const char *str);
int insert_mem(struct gb *b, const char *mem, size_t mem_len);
int insert_file(struct gb *b, const char *fn);
int delete_ch(struct gb *b);
int left_ch(struct gb *b);
int right_ch(struct gb *b);
int backspace_ch(struct gb *b);
void start_of_line(struct gb *b);
void end_of_line(struct gb *b);
int up_line(struct gb *b);
int down_line(struct gb *b);
void left_word(struct gb *b);
void right_word(struct gb *b, char transform);
int goto_row(struct gb *b, struct gb *cl);
int insert_hex(struct gb *b, struct gb *cl);
int swap_cursor_and_mark(struct gb *b);
int exact_forward_search(struct gb *b, struct gb *cl);
int regex_forward_search(struct gb *b, struct gb *cl);
int regex_replace_region(struct gb *b, struct gb *cl);
int match_bracket(struct gb *b);
void trim_clean(struct gb *b);
int copy_region(struct gb *b, struct gb *p, int cut);
int cut_to_eol(struct gb *b, struct gb *p);
int cut_to_sol(struct gb *b, struct gb *p);
int word_under_cursor(struct gb *b, struct gb *tmp);
int copy_logical_line(struct gb *b, struct gb *tmp);
int insert_shell_cmd(struct gb *b, const char *cmd, int *es);
int shell_line(struct gb *b, struct gb *tmp, int *es);
int paste(struct gb *b, struct gb *p);
int save(struct gb *b);
int rename_gb(struct gb *b, const char *cwd, const char *fn);
int new_gb(struct gb **b, const char *cwd, const char *fn, size_t s);
void remove_gb(struct gb **b);
