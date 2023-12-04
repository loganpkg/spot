FILE *fopen_w(const char *fn);
int get_path_type(const char *path, unsigned char *type);
int rec_rm(const char *path);
int insert_ls(struct gb *b, const char *dir);
