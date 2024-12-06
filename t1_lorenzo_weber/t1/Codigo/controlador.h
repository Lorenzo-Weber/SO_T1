#ifndef __CONTROLADOR_H__
#define __CONTROLADOR_H__

#include "es.h"

typedef struct com_t com_t;

com_t *init_com(es_t *es);

void erase_com(com_t *self);
void reserved_com(com_t *self, int id);
void free_com(com_t *self, int id);

int register_com(com_t *self, dispositivo_id_t leitura, dispositivo_id_t leitura_ok, dispositivo_id_t escrita, dispositivo_id_t escrita_ok);
int avaliable_com(com_t *self);

bool read_com(com_t *self, int *value, int id);
bool write_com(com_t *self, int id, int valor);

#endif