#ifndef __PROCESSO_H__
#define __PROCESSO_H__

typedef enum {
    EXEC_PROC,
    READY_PROC,
    BLOCKED_PROC,
    DEAD_PROC,
    N_PROC_STATE
} proc_state_t;

typedef enum {
    PROC_BLOCK_READ,
    PROC_BLOCK_WRITE,
    PROC_BLOCK_WAIT,
    N_PROC_BLOCK
} proc_enum_block_t;

typedef struct proc_t proc_t;
typedef struct proc_state_metrics_t proc_state_metrics_t;
typedef struct proc_metrics_t proc_metrics_t;

struct proc_state_metrics_t
{
    int n;
    int total;
};

struct proc_metrics_t
{
    int n_preempcoes;
    
    int t_return;
    int t_answer;

    proc_state_metrics_t states[N_PROC_STATE];
};

proc_t *proc_init(int id, int end);
void proc_kill(proc_t *self);

int proc_get_id(proc_t *self);
int proc_get_PC(proc_t *self);
int proc_get_A(proc_t *self);
int proc_get_X(proc_t *self);
int proc_get_arg_block(proc_t *self);
double proc_get_priority(proc_t *self);
proc_enum_block_t proc_get_block(proc_t *self);
proc_state_t proc_get_state(proc_t *self);
proc_metrics_t proc_get_metrics(proc_t *self);

void proc_set_priority(proc_t *self, double value);
int proc_get_port(proc_t *self);
void proc_set_port(proc_t *self, int value);
void proc_set_PC(proc_t *self, int valor);
void proc_set_A(proc_t *self, int valor);
void proc_set_X(proc_t *self, int valor);

void proc_execute(proc_t *self);
void proc_stop(proc_t *self);
void proc_block(proc_t *self, proc_enum_block_t motivo, int arg);
void proc_unblock(proc_t *self);
void proc_death_state(proc_t *self);

void proc_unset_port(proc_t *self);

void proc_update_metrics(proc_t *self, int delta);

char *proc_state2string(proc_state_t estado);

#endif