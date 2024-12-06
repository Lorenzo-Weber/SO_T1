#include <stdlib.h>
#include "escalonador.h"

typedef struct esc_no_t esc_no_t;

typedef struct esc_no_t {
  proc_t *proc;

  esc_no_t *previous;
  esc_no_t *prox;
}esc_no_t;

typedef struct esc_t
{
  esc_mode mode;

  esc_no_t *ini;
  esc_no_t *end;
}esc_t;

static proc_t *next_simple_esc(esc_t *self);
static proc_t *next_robin_esc(esc_t *self);
static proc_t *next_prioritary_esc(esc_t *self);

esc_t *init_esc(esc_mode type)
{
  esc_t *self = (esc_t *)malloc(sizeof(esc_t));

  self->mode = type;
  self->ini = NULL;
  self->end = NULL;

  return self;
}

void erase_esc(esc_t *self)
{
  while (self->ini != NULL) {
    esc_no_t *prox = self->ini->prox;
    free(self->ini);
    self->ini = prox;
  }

  free(self);
}

void insert_esc(esc_t *self, proc_t *p)
{
  esc_no_t *no = (esc_no_t *)malloc(sizeof(esc_no_t));

  no->proc = p;
  no->previous = self->end;
  no->prox = NULL;

  if (self->end == NULL) {
    self->ini = no;
  } else {
    self->end->prox = no;
  }

  self->end = no;
}

void delete_esc(esc_t *self, proc_t *p)
{
  for (esc_no_t *no = self->ini; no != NULL; no = no->prox) {
    if (no->proc != p) {
      continue;
    }

    if (no->previous != NULL) {
      no->previous->prox = no->prox;
    } else {
      self->ini = no->prox;
    }

    if (no->prox != NULL) {
      no->prox->previous = no->previous;
    } else {
      self->end = no->previous;
    }

    free(no);

    break;
  }
}

proc_t *next_esc(esc_t *self)
{
  switch (self->mode)
  {
  case SIMPLE_ESC:
    return next_simple_esc(self);
  case ROBIN_ESC:
    return next_robin_esc(self);
  case PRIORITARY_ESC:
    return next_prioritary_esc(self);
  default:
    return NULL;
  }
}

static proc_t *next_simple_esc(esc_t *self)
{
  if (self->ini == NULL) {
    return NULL;
  }

  for (esc_no_t *no = self->ini; no != NULL; no = no->prox) {
    if (proc_get_state(no->proc) == EXEC_PROC) {
      return no->proc;
    }
  }

  return self->ini->proc;
}

static proc_t *next_robin_esc(esc_t *self)
{
  if (self->ini == NULL) {
    return NULL;
  }

  return self->ini->proc;
}

static proc_t *next_prioritary_esc(esc_t *self)
{
  if (self->ini == NULL) {
    return NULL;
  }

  proc_t *new = self->ini->proc;

  for (esc_no_t *no = self->ini->prox; no != NULL; no = no->prox) {
    if (proc_get_priority(no->proc) < proc_get_priority(new)) {
      new = no->proc;
    }
  }

  return new;
}