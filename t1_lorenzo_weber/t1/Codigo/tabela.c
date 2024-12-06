#include "tabela.h"
#include "console.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

typedef struct table_t {
  int rows;
  int cols;
  int cell_size;

  char *cells;
  int *widths;
}table_t;

static int table_size(table_t *self);
static char *table_cell(table_t *self, int i, int j);

table_t *table_init(int rows, int cols, int cell_size)
{
    table_t *self = malloc(sizeof(*self));
    assert(self != NULL);
    
    self->rows = rows;
    self->cols = cols;
    self->cell_size = cell_size;
    
    self->cells = calloc(rows * cols, cell_size * sizeof(char));
    assert(self->cells != NULL);

    self->widths = calloc(cols, sizeof(int));
    assert(self->widths != NULL);
    
    return self;
}

void table_free(table_t *self)
{
    free(self->cells);
    free(self->widths);
    free(self);
}

int table_get_rows(table_t *self)
{
    return self->rows;
}

int table_get_cols(table_t *self)
{
    return self->cols;
}

void table_fill(table_t *self, int i, int j, const char *shape, ...)
{
    char *celula = table_cell(self, i, j);

    va_list arg;
    va_start(arg, shape);
    vsnprintf(celula, self->cell_size, shape, arg);
    va_end(arg);

    int width = strlen(celula);

    if (width > self->widths[j]) {
        self->widths[j] = width;
    }
}

void table_print(table_t *self)
{
    char *buffer = (char *)malloc(table_size(self) * sizeof(char));
    char *ptr = buffer;

    for (int j = 0; j < self->cols; j++) {
        ptr += sprintf(ptr, "| %-*s ", self->widths[j], table_cell(self, 0, j));
    }

    ptr += sprintf(ptr, "| \n");

    for (int j = 0; j < self->cols; j++) {
        ptr += sprintf(ptr, "| ");

        for (int k = 0; k < self->widths[j]; k++) {
            *ptr++ = '-';
        }

        *ptr++ = ' ';
    }

    ptr += sprintf(ptr, "|\n");

    for (int i = 1; i < self->rows; i++) {
        for (int j = 0; j < self->cols; j++) {
            char *celula = table_cell(self, i, j);
            int width = self->widths[j];

            ptr += sprintf(ptr, "| %-*s ", width, celula);
        }

        ptr += sprintf(ptr, "|\n");
    }

    console_printf("%s\n", buffer);

    free(buffer);
}

static int table_size(table_t *self)
{
    int size = 0;

    for (int j = 0; j < self->cols; j++) {
        size += self->widths[j] + 2;
    }

    size += self->cols + 2;
    size *= self->rows + 1;

    return size + 1;
}

static char *table_cell(table_t *self, int i, int j)
{
    return self->cells + (i * self->cols + j) * self->cell_size;
}