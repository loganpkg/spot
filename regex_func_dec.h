int regex_search(const char *mem, size_t mem_len,
                 const char *regex_find_str, int sol,
                 int nl_sen, size_t * match_offset, size_t * match_len);
int regex_replace(const char *mem, size_t mem_len,
                  const char *regex_find_str, const char *replace,
                  size_t replace_len, int nl_sen, char **res,
                  size_t * res_len, int verbose);
