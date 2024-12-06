#ifndef __ESCALONADOR_H__
#define __ESCALONADOR_H__

#include <stdbool.h>
#include "processo.h"

typedef struct esc_t esc_t;

typedef enum {
    SIMPLE_ESC,
    ROBIN_ESC,
    PRIORITARY_ESC,
    ESC_MODE
} esc_mode;

esc_t *init_esc(esc_mode type);
proc_t *next_esc(esc_t *self);

void erase_esc(esc_t *self);
void insert_esc(esc_t *self, proc_t *proc);
void delete_esc(esc_t *self, proc_t *proc);

#endif