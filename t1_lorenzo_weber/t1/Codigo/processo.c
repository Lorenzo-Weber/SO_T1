#include <stdlib.h>
#include "processo.h"

typedef struct proc_t
{
  int id;

  proc_state_t state;

  double priority;

  int enum_bloq;
  int arg_block;
  int port;

  int reg_PC;
  int reg_A;
  int reg_X;

  proc_metrics_t metrics;
} proc_t;

static void proc_change_state(proc_t *self, proc_state_t state);

proc_t *proc_init(int id, int end)
{
  proc_t *self = malloc(sizeof(proc_t));

  self->id = id;
  self->state = READY_PROC;
  self->priority = 0.5;

  self->enum_bloq = 0;
  self->arg_block = 0;
  self->port = -1;

  self->reg_PC = end;
  self->reg_A = 0;
  self->reg_X = 0;

  self->metrics.t_return = 0;
  self->metrics.n_preempcoes = 0;

  for (int i = 0; i < N_PROC_STATE; i++)
  {
    self->metrics.states[i].n = 0;
    self->metrics.states[i].total = 0;
  }

  self->metrics.states[READY_PROC].n = 1;

  return self;
}

int proc_get_id(proc_t *self) { return self->id; }
int proc_get_PC(proc_t *self) { return self->reg_PC; }
int proc_get_A(proc_t *self) { return self->reg_A; }
int proc_get_X(proc_t *self) { return self->reg_X; }
int proc_get_arg_block(proc_t *self) { return self->arg_block; }
int proc_get_port(proc_t *self) { return self->port; }
double proc_get_priority(proc_t *self) { return self->priority; }
proc_state_t proc_get_state(proc_t *self) { return self->state; }
proc_metrics_t proc_get_metrics(proc_t *self) { return self->metrics; } 
proc_enum_block_t proc_get_block(proc_t *self) { return self->enum_bloq; }

void proc_set_priority(proc_t *self, double value) { self->priority = value; }
void proc_set_PC(proc_t *self, int value) { self->reg_PC = value; }
void proc_set_A(proc_t *self, int value) { self->reg_A = value; }
void proc_set_X(proc_t *self, int value) { self->reg_X = value; }
void proc_set_port(proc_t *self, int value) { self->port = value; }

void proc_kill(proc_t *self) { free(self); }

void proc_execute(proc_t *self) { proc_change_state(self, EXEC_PROC); }

void proc_stop(proc_t *self) { proc_change_state(self, READY_PROC); }

void proc_unblock(proc_t *self) { proc_change_state(self, READY_PROC); }

void proc_death_state(proc_t *self) { proc_change_state(self, DEAD_PROC); }

void proc_unset_port(proc_t *self) { self->port = -1; }

void proc_block(proc_t *self, proc_enum_block_t reason, int arg) {
  proc_change_state(self, BLOCKED_PROC);

  self->enum_bloq = reason;
  self->arg_block = arg;
}

void proc_update_metrics(proc_t *self, int value) {
  if (self->state != DEAD_PROC) self->metrics.t_return += value;

  self->metrics.states[self->state].total += value;
  self->metrics.t_answer = self->metrics.states[READY_PROC].total;
  self->metrics.t_answer /= self->metrics.states[READY_PROC].n;
}

char *proc_state2string(proc_state_t state) {
  switch (state)
  {
  case READY_PROC:
    return "READY";
  case EXEC_PROC:
    return "RUNNING";
  case BLOCKED_PROC:
    return "BLOCKED";
  case DEAD_PROC:
    return "DEAD";
  default:
    return "UNKNOW STATE";
  }
}

static void proc_change_state(proc_t *self, proc_state_t state) {
  if (self->state == EXEC_PROC && state == READY_PROC)
  {
    self->metrics.n_preempcoes++;
  }

  self->metrics.states[state].n++;
  self->state = state;
}
