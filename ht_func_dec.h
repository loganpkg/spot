struct ht *init_ht(size_t num_buckets);
void free_ht(struct ht *ht);
struct entry *lookup(struct ht *ht, const char *name);
int delete_entry(struct ht *ht, const char *name);
int upsert(struct ht *ht, const char *name, const char *def, Fptr func_p);
