int eval(struct ibuf *input, int read_stdin, int *math_error, long *res,
         int verbose);
int eval_str(const char *math_str, long *res, int verbose);
