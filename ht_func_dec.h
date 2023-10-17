struct entry **init_ht(void);
void free_ht(struct entry **ht);
struct entry *lookup(struct entry **ht, const char *name);
int delete_entry(struct entry **ht, const char *name);
int upsert(struct entry **ht, const char *name, const char *def,
           Fptr func_p);
