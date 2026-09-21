#include "table.h"
#include <stdlib.h>
#include <string.h>

void table_init(struct table* t, int capacity, table_hash_func h, table_compare_func c) {
  t->count = 0; t->capacity = capacity; t->max_load = 0.75f;
  t->hash = h; t->cmp = c;
  t->buckets = (struct bucket**)malloc(sizeof(struct bucket*) * capacity);
  memset(t->buckets, 0, sizeof(struct bucket*) * capacity);
}
void table_free(struct table* t) {
  for (int i = 0; i < t->capacity; ++i) {
    struct bucket *n, *b = t->buckets[i];
    while (b) { n = b->next; free(b->key); free(b); b = n; }
  }
  if (t->buckets) { free(t->buckets); t->buckets = NULL; }
}
void table_clear(struct table* t) {
  table_hash_func* h = t->hash; table_compare_func* c = t->cmp;
  int cap = t->capacity; table_free(t); table_init(t, cap, h, c);
}
static struct bucket** get_bucket(struct table* t, void* key) {
  struct bucket** b = t->buckets + (t->hash(key) % (unsigned)t->capacity);
  while (*b) { if (t->cmp((*b)->key, key)) break; b = &(*b)->next; }
  return b;
}
static void rehash(struct table* t) {
  struct bucket** old = t->buckets; int ocap = t->capacity;
  t->count = 0; t->capacity *= 2;
  t->buckets = (struct bucket**)malloc(sizeof(struct bucket*) * t->capacity);
  memset(t->buckets, 0, sizeof(struct bucket*) * t->capacity);
  for (int i = 0; i < ocap; ++i) {
    struct bucket *nb, *ob = old[i];
    while (ob) {
      struct bucket** dst = get_bucket(t, ob->key);
      *dst = (struct bucket*)malloc(sizeof(struct bucket));
      (*dst)->key = ob->key; (*dst)->value = ob->value; (*dst)->next = NULL;
      ++t->count; nb = ob->next; free(ob); ob = nb;
    }
  }
  free(old);
}
void _table_add(struct table* t, void* key, int key_size, void* value) {
  struct bucket** b = get_bucket(t, key);
  if (*b) { if (!(*b)->value) (*b)->value = value; }
  else {
    *b = (struct bucket*)malloc(sizeof(struct bucket));
    (*b)->key = malloc(key_size); (*b)->value = value;
    memcpy((*b)->key, key, key_size); (*b)->next = NULL; ++t->count;
    if ((1.0f * t->count) / t->capacity > t->max_load) rehash(t);
  }
}
void table_remove(struct table* t, void* key) {
  struct bucket *n, **b = get_bucket(t, key);
  if (*b) { free((*b)->key); n = (*b)->next; free(*b); *b = n; --t->count; }
}
void* table_find(struct table* t, void* key) {
  struct bucket* b = *get_bucket(t, key);
  return b ? b->value : NULL;
}
