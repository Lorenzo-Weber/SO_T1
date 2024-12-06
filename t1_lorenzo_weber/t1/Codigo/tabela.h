#ifndef __TABELA_H__
#define __TABELA_H__

typedef struct table_t table_t;

table_t *table_init(int n_linhas, int n_colunas, int tam_celula);
void table_free(table_t *self);

int table_get_rows(table_t *self);
int table_get_cols(table_t *self);

void table_fill(table_t *self, int i, int j, const char *shape, ...);

void table_print(table_t *self);

#endif