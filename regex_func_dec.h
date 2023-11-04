void print_regex(unsigned char *char_sets,
                 size_t *regex_nums, size_t rn_len);
int regex_replace(const char *mem, size_t mem_len,
                  const char *regex_find_str, const char *replace,
                  size_t replace_len, int nl_sen, char **res,
                  size_t *res_len);
