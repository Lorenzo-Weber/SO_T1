#include <stdlib.h>
#include "controlador.h"

typedef struct port_t port_t;

#define ini_port_size 4

struct port_t
{
	dispositivo_id_t read;
	dispositivo_id_t write;
	dispositivo_id_t read_ok;
	dispositivo_id_t write_ok;

	bool reserved;
};

typedef struct com_t
{
	es_t *es;

	int size;
	int qtd;
	int ini;

	port_t *ports;
} com_t;

com_t *init_com(es_t *es)
{
	com_t *self = (com_t *)malloc(sizeof(com_t));

	self->es = es;
	self->size = ini_port_size;
	self->qtd = 0;
	self->ini = 0;

	self->ports = (port_t *)malloc(sizeof(port_t) * self->size);

	return self;
}

void erase_com(com_t *self)
{
	free(self->ports);
	free(self);
}

void reserved_com(com_t *self, int id)
{
	self->ports[id].reserved = true;
}

void free_com(com_t *self, int id)
{
	self->ports[id].reserved = false;
}

int register_com(com_t *self, dispositivo_id_t read, dispositivo_id_t read_ok, dispositivo_id_t write, dispositivo_id_t write_ok)
{
	if (self->qtd == self->size) {
		self->size *= 2;
		self->ports = (port_t *)realloc(self->ports, sizeof(port_t) * self->size);
	}

	self->ports[self->qtd].read = read;
	self->ports[self->qtd].write = write;
	self->ports[self->qtd].read_ok = read_ok;
	self->ports[self->qtd].write_ok = write_ok;

	self->ports[self->qtd].reserved = false;

	return self->qtd++;
}

int avaliable_com(com_t *self)
{
	for (int j = 0; j < self->qtd; j++) {

		int i = (self->ini + j) % self->qtd;
		if (!self->ports[i].reserved) {
			self->ini = (i + 1) % self->qtd;
			return i;
		}
	}

	return -1;
}

bool read_com(com_t *self, int *value, int id)
{
	int ok;

	if (es_le(self->es, self->ports[id].read_ok, &ok) != ERR_OK) return false;
	if (!ok) return false;

	return es_le(self->es, self->ports[id].read, value) == ERR_OK;
}

bool write_com(com_t *self, int id, int valor)
{
	int ok;

	if (es_le(self->es, self->ports[id].write_ok, &ok) != ERR_OK) return false;
	if (!ok) return false;

	return es_escreve(self->es, self->ports[id].write, valor) == ERR_OK;
}