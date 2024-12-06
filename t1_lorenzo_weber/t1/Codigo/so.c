#include "so.h"
#include "dispositivos.h"
#include "irq.h"
#include "programa.h"
#include "instrucao.h"
#include "controlador.h"
#include "escalonador.h"
#include "tabela.h"

#include <stdlib.h>
#include <stdbool.h>

#define INTERVALO_INTERRUPCAO 20   

#define TABLE_SIZE 16
#define ESC_MODE PRIORITARY_ESC // Switch to PRIORITARY_ESC or ROBIN_ESC or SIMPLE_ESC
#define QUANTUM 10 

typedef struct so_metrics_t {
  int execution_time;
  int stop_time;

  int preempcoes;

  int n_irqs[N_IRQ];
}so_metrics_t;

struct so_t {
  cpu_t *cpu;
  mem_t *mem;
  es_t *es;
  console_t *console;
  com_t *com;
  esc_t *esc;

  int proc_size;
  int proc_qtd;

  proc_t *proc;
  proc_t **proc_table;

  int proc_quantum;
  int remaining_procs;

  int clock;

  so_metrics_t metrics;

  bool erro_interno;
};


static int so_trata_interrupcao(void *argC, int reg_A);
static int so_carrega_programa(so_t *self, char *nome_do_executavel);
static bool copia_str_da_mem(int tam, char str[tam], mem_t *mem, int ender);

static proc_t *so_proc_search(so_t *self, int pid);
static proc_t *so_init_proc(so_t *self, char *nome_do_executavel);
static void so_proc_kill(so_t *self, proc_t *proc);
static void so_proc_unlock(so_t *self, proc_t *proc);
static void so_proc_port(so_t *self, proc_t *proc);
static bool so_esc(so_t *self);
static bool so_proc_queue(so_t *self);
static void so_metrics_print(so_t *self);
static void so_metrics_update(so_t *self, int delta);
static void so_proc_execute(so_t *self, proc_t *proc);
static void so_proc_block(so_t *self, proc_t *proc, proc_enum_block_t reason, int arg);

so_t *so_cria(cpu_t *cpu, mem_t *mem, es_t *es, console_t *console)
{
  so_t *self = malloc(sizeof(*self));
  if (self == NULL) return NULL;

  self->cpu = cpu;
  self->mem = mem;
  self->es = es;
  self->console = console;
  self->esc = init_esc(ESC_MODE);
  self->com = init_com(es);

  self->proc_size = TABLE_SIZE;
  self->proc_qtd = 0;

  self->proc = NULL;
  self->proc_table = malloc(self->proc_size * sizeof(proc_t *));

  self->proc_quantum = QUANTUM;
  self->remaining_procs = 0;

  self->clock = -1;

  self->metrics.execution_time = 0;
  self->metrics.stop_time = 0;

  self->metrics.preempcoes = 0;

  for (int i = 0; i < N_IRQ; i++) {
    self->metrics.n_irqs[i] = 0;
  }

  self->erro_interno = false;

  register_com(self->com, D_TERM_A_TECLADO, D_TERM_A_TECLADO_OK, D_TERM_A_TELA, D_TERM_A_TELA_OK);
  register_com(self->com, D_TERM_B_TECLADO, D_TERM_B_TECLADO_OK, D_TERM_B_TELA, D_TERM_B_TELA_OK);
  register_com(self->com, D_TERM_C_TECLADO, D_TERM_C_TECLADO_OK, D_TERM_C_TELA, D_TERM_C_TELA_OK);
  register_com(self->com, D_TERM_D_TECLADO, D_TERM_D_TECLADO_OK, D_TERM_D_TELA, D_TERM_D_TELA_OK);

  cpu_define_chamaC(self->cpu, so_trata_interrupcao, self);

  int ender = so_carrega_programa(self, "trata_int.maq");
  if (ender != IRQ_END_TRATADOR) {
    console_printf("SO: problema na carga do programa de tratamento de interrupcao");
    self->erro_interno = true;
  }

  if (es_escreve(self->es, D_RELOGIO_TIMER, INTERVALO_INTERRUPCAO) != ERR_OK) {
    console_printf("SO: problema na programacao do timer");
    self->erro_interno = true;
  }

  return self;
}

void so_destroi(so_t *self)
{
  cpu_define_chamaC(self->cpu, NULL, NULL);
  erase_com(self->com);
  erase_esc(self->esc);

  for (int i = 0; i < self->proc_qtd; i++) {
    proc_kill(self->proc_table[i]);
  }

  free(self->proc_table);
  free(self);
}

static void so_salva_estado_da_cpu(so_t *self);
static void so_trata_irq(so_t *self, int irq);
static void so_escalona(so_t *self);
static int so_despacha(so_t *self);
static void so_trata_pendencias(so_t *self);
static void so_sync(so_t *self);
static void so_block_handler(so_t *self, proc_t *proc);
static int poweroff(so_t *self);

static int so_trata_interrupcao(void *argC, int reg_A)
{
  so_t *self = argC;
  irq_t irq = reg_A;

  self->metrics.n_irqs[irq]++;

  so_salva_estado_da_cpu(self);
  so_sync(self);
  so_trata_irq(self, irq);
  so_trata_pendencias(self);
  so_escalona(self);

  if (so_proc_queue(self)) return so_despacha(self);
  else return poweroff(self);
  
}

static void so_salva_estado_da_cpu(so_t *self)
{
  proc_t *proc = self->proc;

  if (proc == NULL) {
    return;
  }
  
  int PC, A, X;
  mem_le(self->mem, IRQ_END_PC, &PC);
  mem_le(self->mem, IRQ_END_A, &A);
  mem_le(self->mem, IRQ_END_X, &X);

  proc_set_PC(proc, PC);
  proc_set_A(proc, A);
  proc_set_X(proc, X);
}

static void so_sync(so_t *self)
{
  int last_clock = self->clock;

  if (es_le(self->es, D_RELOGIO_INSTRUCOES, &self->clock) != ERR_OK) {
    console_printf("SO: problema na leitura do relogio");
    return;
  }

  if (last_clock == -1) {
    return;
  }

  int delta = self->clock - last_clock;

  so_metrics_update(self, delta);
}

static void so_trata_pendencias(so_t *self)
{
  for (int i = 0; i < self->proc_qtd; i++) {
    proc_t *proc = self->proc_table[i];

    if (proc_get_state(proc) == BLOCKED_PROC) {
      so_block_handler(self, proc);
    }
  }
}

static void so_escalona(so_t *self)
{
  if (!so_esc(self)) {
    return;
  }

  if (self->proc != NULL) {
    int execution_time = self->proc_quantum - self->remaining_procs;

    double prioridade = proc_get_priority(self->proc);
    prioridade += (double)execution_time / self->proc_quantum;
    prioridade /= 2.0;

    console_printf(
      "SO: PROCCESS %d - PRIORITY: %lf -> %lf",
      proc_get_id(self->proc),
      proc_get_priority(self->proc),
      prioridade
    );

    proc_set_priority(self->proc, prioridade);
  }

  if (
    self->proc != NULL &&
    proc_get_state(self->proc) == EXEC_PROC
  ) {
    insert_esc(self->esc, self->proc);
  }

  proc_t *proc = next_esc(self->esc);

  if (proc != NULL) {
    console_printf("SO: PROCCESS ESC %d", proc_get_id(proc));
  } else {
    console_printf("SO: EMPTY QUEUE");
  }

  so_proc_execute(self, proc);
}

static int so_despacha(so_t *self)
{
  if (self->erro_interno) return 1;

  proc_t *proc = self->proc;

  if (proc == NULL) {
    return 1;
  }
  
  int PC = proc_get_PC(proc);
  int A = proc_get_A(proc);
  int X = proc_get_X(proc);

  mem_escreve(self->mem, IRQ_END_PC, PC);
  mem_escreve(self->mem, IRQ_END_A, A);
  mem_escreve(self->mem, IRQ_END_X, X);

  return 0;
}

static int poweroff(so_t *self)
{
  err_t e1, e2;
  e1 = es_escreve(self->es, D_RELOGIO_INTERRUPCAO, 0);
  e2 = es_escreve(self->es, D_RELOGIO_TIMER, 0);

  if (e1 != ERR_OK || e2 != ERR_OK) {
    console_printf("SO: TIMER FAULT");
    self->erro_interno = true;
  }

  so_metrics_print(self);

  return 1;
}

static void so_block_handler_write(so_t *self, proc_t *proc);
static void so_block_handler_read(so_t *self, proc_t *proc);
static void so_block_handler_proc_await(so_t *self, proc_t *proc);
static void so_block_handler_default(so_t *self, proc_t *proc);

static void so_block_handler(so_t *self, proc_t *proc)
{
  proc_enum_block_t reason = proc_get_block(proc);

  switch (reason)
  {
  case PROC_BLOCK_READ:
    so_block_handler_write(self, proc);
    break;
  case PROC_BLOCK_WRITE:
    so_block_handler_read(self, proc);
    break;
  case PROC_BLOCK_WAIT:
    so_block_handler_proc_await(self, proc);
    break;
  default:
    so_block_handler_default(self, proc);
    break;
  }
}

static void so_block_handler_write(so_t *self, proc_t *proc)
{
  so_proc_port(self, proc);

  int porta = proc_get_port(proc);
  int dado;

  if (porta != -1 && read_com(self->com, &dado, porta )) {
    proc_set_A(proc, dado);
    so_proc_unlock(self, proc);

    console_printf("SO: PROCCESS %d - READING UNLOCKED", proc_get_id(proc));
  }
}

static void so_block_handler_read(so_t *self, proc_t *proc)
{
  so_proc_port(self, proc);

  int porta = proc_get_port(proc);
  int dado = proc_get_arg_block(proc);

  if (porta != -1 && write_com(self->com, porta, dado)) {
    proc_set_A(proc, 0);
    so_proc_unlock(self, proc);

    console_printf("SO: PROCCESS %d - WRITING UNLOCKED", proc_get_id(proc));
  }
}

static void so_block_handler_proc_await(so_t *self, proc_t *proc)
{
  int pid_alvo = proc_get_arg_block(proc);
  proc_t *proc_alvo = so_proc_search(self, pid_alvo);

  if (proc_alvo == NULL || proc_get_state(proc_alvo) == DEAD_PROC) {
    proc_set_A(proc, 0);
    so_proc_unlock(self, proc);

    console_printf(
      "SO: PROCCESS %d - AWAITING UNLOCKED",
      proc_get_id(proc)
    );
  }
}

static void so_block_handler_default(so_t *self, proc_t *proc)
{
  console_printf("SO: UNKNOW BLOCK: %d", proc_get_block(proc));
  self->erro_interno = true;
}

static void so_trata_irq_reset(so_t *self);
static void so_trata_irq_chamada_sistema(so_t *self);
static void so_trata_irq_err_cpu(so_t *self);
static void so_trata_irq_relogio(so_t *self);
static void so_trata_irq_desconhecida(so_t *self, int irq);

static void so_trata_irq(so_t *self, int irq)
{
  switch (irq) {
    case IRQ_RESET:
      so_trata_irq_reset(self);
      break;
    case IRQ_SISTEMA:
      so_trata_irq_chamada_sistema(self);
      break;
    case IRQ_ERR_CPU:
      so_trata_irq_err_cpu(self);
      break;
    case IRQ_RELOGIO:
      so_trata_irq_relogio(self);
      break;
    default:
      so_trata_irq_desconhecida(self, irq);
  }
}

static void so_trata_irq_reset(so_t *self)
{

  proc_t *proc = so_init_proc(self, "init.maq");

  if (proc_get_PC(proc) != 100) {
    console_printf("SO: problema na carga do programa inicial");
    self->erro_interno = true;
    return;
  }

  mem_escreve(self->mem, IRQ_END_modo, usuario);
}

static void so_trata_irq_err_cpu(so_t *self)
{
  int err_int;
  mem_le(self->mem, IRQ_END_erro, &err_int);
  err_t err = err_int;
  console_printf("SO: IRQ nao tratada -- erro na CPU: %s", err_nome(err));
  self->erro_interno = true;
}

static void so_trata_irq_relogio(so_t *self)
{
  err_t e1, e2;
  e1 = es_escreve(self->es, D_RELOGIO_INTERRUPCAO, 0); // desliga o sinalizador de interrupcao
  e2 = es_escreve(self->es, D_RELOGIO_TIMER, INTERVALO_INTERRUPCAO);
  if (e1 != ERR_OK || e2 != ERR_OK) {
    console_printf("SO: problema da reinicializacao do timer");
    self->erro_interno = true;
  }

  console_printf("SO: interrupcao do relogio");

  if (self->remaining_procs > 0) {
    self->remaining_procs--;
  }
}

static void so_trata_irq_desconhecida(so_t *self, int irq)
{
  console_printf("SO: nao sei tratar IRQ %d (%s)", irq, irq_nome(irq));
  self->erro_interno = true;
}

static void so_chamada_le(so_t *self);
static void so_chamada_escr(so_t *self);
static void so_chamada_cria_proc(so_t *self);
static void so_chamada_mata_proc(so_t *self);
static void so_chamada_espera_proc(so_t *self);

static void so_trata_irq_chamada_sistema(so_t *self)
{
  int id_chamada;
  if (mem_le(self->mem, IRQ_END_A, &id_chamada) != ERR_OK) {
    console_printf("SO: erro no acesso ao id da chamada de sistema");
    self->erro_interno = true;
    return;
  }
  switch (id_chamada) {
    case SO_LE:
      so_chamada_le(self);
      break;
    case SO_ESCR:
      so_chamada_escr(self);
      break;
    case SO_CRIA_PROC:
      so_chamada_cria_proc(self);
      break;
    case SO_MATA_PROC:
      so_chamada_mata_proc(self);
      break;
    case SO_ESPERA_PROC:
      so_chamada_espera_proc(self);
      break;
    default:
      console_printf("SO: chamada de sistema desconhecida (%d)", id_chamada);
      self->erro_interno = true;
  }
}

static void so_chamada_le(so_t *self)
{
  proc_t *proc = self->proc;

  if (proc == NULL) {
    return;
  }
  
  so_proc_port(self, proc);

  int porta = proc_get_port(proc);
  int dado;

  if (porta != -1 && read_com(self->com, &dado, porta )) {
    proc_set_A(proc, dado);
  } else {
    console_printf("SO: PROCCESS %d - READING BLOCKED", proc_get_id(proc));
    so_proc_block(self, proc, PROC_BLOCK_READ, 0);
  }
}

static void so_chamada_escr(so_t *self)
{
  proc_t *proc = self->proc;

  if (proc == NULL) {
    return;
  }
  so_proc_port(self, proc);

  int porta = proc_get_port(proc);
  int dado = proc_get_X(proc);

  if (porta != -1 && write_com(self->com, porta, dado)) {
    proc_set_A(proc, 0);
  } else {
    console_printf("SO: PROCCESS %d - WRITING BLOCKED", proc_get_id(proc));
    so_proc_block(self, proc, PROC_BLOCK_WRITE, dado);
  }
}

static void so_chamada_cria_proc(so_t *self)
{
  proc_t *proc = self->proc;

  if (proc == NULL) {
    return;
  }

  int ender_proc = proc_get_X(proc);
  char nome[100];

  if (copia_str_da_mem(100, nome, self->mem, ender_proc)) {
    console_printf(
      "SO: PROCCESS %d - cria PROCCESS (nome: %s)",
      proc_get_id(self->proc),
      nome
    );

    proc_t *proc_alvo = so_init_proc(self, nome);

    if (proc_alvo != NULL) {
      proc_set_A(proc, proc_get_id(proc_alvo));
      return;
    } 
  }
  proc_set_A(proc, -1);
}

static void so_chamada_mata_proc(so_t *self)
{
  proc_t *proc = self->proc;

  if (proc == NULL) {
    return;
  }

  int pid_alvo = proc_get_X(proc);
  proc_t *proc_alvo = so_proc_search(self, pid_alvo);

  console_printf(
    "SO: PROCCESS %d - KILL PROCCESS (PID: %d)",
    proc_get_id(self->proc),
    pid_alvo
  );

  if (pid_alvo == 0) {
    proc_alvo = self->proc;
  }
  
  if (proc_alvo != NULL) {
    so_proc_kill(self, proc_alvo);
    proc_set_A(proc, 0);
  } else {
    proc_set_A(proc, -1);
  }
}

static void so_chamada_espera_proc(so_t *self)
{
  proc_t *proc = self->proc;

  if (proc == NULL) {
    return;
  }

  int pid_alvo = proc_get_X(proc);
  proc_t *proc_alvo = so_proc_search(self, pid_alvo);

  console_printf(
    "SO: PROCCESS %d - AWAIT PROCCESS (PID: %d)",
    proc_get_id(self->proc),
    pid_alvo
  );

  if (proc_alvo == NULL || proc_alvo == proc) {
    proc_set_A(proc, -1);
    return;
  }

  if (proc_get_state(proc_alvo) == DEAD_PROC) {
    proc_set_A(proc, 0);
  } else {
    console_printf(
    "SO: PROCCESS %d - PROCCESS AWAIT BLOCK",
     proc_get_id(proc)
    );

    so_proc_block(self, proc, PROC_BLOCK_WAIT, pid_alvo);
  }
}

static proc_t *so_proc_search(so_t *self, int pid)
{
  if (pid <= 0 || pid > self->proc_qtd) {
    return NULL;
  }

  return self->proc_table[pid - 1];
}

static proc_t *so_init_proc(so_t *self, char *nome_do_executavel)
{
  int end = so_carrega_programa(self, nome_do_executavel);

  if (end <= 0) {
    return NULL;
  }

  if (self->proc_qtd == self->proc_size) {
    self->proc_size = self->proc_size * 2;
    self->proc_table = realloc(self->proc_table, self->proc_size * sizeof(*self->proc_table));
  }

  proc_t *proc = proc_init(self->proc_qtd + 1, end);

  self->proc_table[self->proc_qtd++] = proc;
  insert_esc(self->esc, proc);

  return proc;
}

static void so_proc_kill(so_t *self, proc_t *proc)
{
  int porta = proc_get_port(proc);

  if (porta != -1) {
    proc_unset_port(proc);
    free_com(self->com, porta);
  }

  delete_esc(self->esc, proc);
  proc_death_state(proc);
}

static void so_proc_execute(so_t *self, proc_t *proc)
{
  if (
    self->proc != NULL &&
    self->proc != proc &&
    proc_get_state(self->proc) == EXEC_PROC
  ) {
    proc_stop(self->proc);
    self->metrics.preempcoes++;
  }

  if (proc != NULL && proc_get_state(proc) != EXEC_PROC) {
    proc_execute(proc);
  }

  if (proc != NULL) {
    delete_esc(self->esc, proc);
  }

  self->proc = proc;
  self->remaining_procs = self->proc_quantum;
}

static void so_proc_block(so_t *self, proc_t *proc, proc_enum_block_t reason, int arg)
{
  delete_esc(self->esc, proc);
  proc_block(proc, reason, arg);
}

static void so_proc_unlock(so_t *self, proc_t *proc)
{
  proc_unblock(proc);
  insert_esc(self->esc, proc);
}

static void so_proc_port(so_t *self, proc_t *proc)
{
  if (proc_get_port(proc) != -1) {
    return;
  }

  int porta = avaliable_com(self->com);
  proc_set_port(proc, porta);
  reserved_com(self->com, porta);
}

static bool so_esc(so_t *self)
{
  if (self->proc == NULL) {
    return true;
  }

  if (proc_get_state(self->proc) != EXEC_PROC) {
    return true;
  }

  if (self->remaining_procs <= 0) {
    return true;
  }

  return false;
}

static bool so_proc_queue(so_t *self)
{
  for (int i = 0; i < self->proc_qtd; i++) {
    if (proc_get_state(self->proc_table[i]) != DEAD_PROC) {
      return true;
    }
  }

  return false;
}

static void so_metrics_update(so_t *self, int delta)
{
  self->metrics.execution_time += delta;
  
  if (self->proc == NULL) {
    self->metrics.stop_time += delta;
  }

  for (int i = 0; i < self->proc_qtd; i++) {
    proc_update_metrics(self->proc_table[i], delta);
  }
}

static void so_metrics_print(so_t *self) {
  const int width = 512;

  table_t *so_metrics_t = table_init(2, 4, width);
  
  table_fill(so_metrics_t, 0, 0, "PROCS");
  table_fill(so_metrics_t, 0, 1, "EXEC");
  table_fill(so_metrics_t, 0, 2, "STOP");
  table_fill(so_metrics_t, 0, 3, "PREEMPCOES");
  table_fill(so_metrics_t, 1, 0, "%d", self->proc_qtd);
  table_fill(so_metrics_t, 1, 1, "%d", self->metrics.execution_time);
  table_fill(so_metrics_t, 1, 2, "%d", self->metrics.stop_time);
  table_fill(so_metrics_t, 1, 3, "%d", self->metrics.preempcoes);

  table_t *irqs_table = table_init(N_IRQ + 1, 2, width);

  table_fill(irqs_table, 0, 0, "IRQ");
  table_fill(irqs_table, 0, 1, "TIMES");

  for (int i = 0; i < N_IRQ; i++) {
    table_fill(irqs_table, i + 1, 0, irq_nome(i));
    table_fill(irqs_table, i + 1, 1, "%d", self->metrics.n_irqs[i]);
  }

  table_t *tabela_proc_geral = table_init(self->proc_qtd + 1, 4, width);
  table_t *tabela_proc_est_vezes = table_init(self->proc_qtd + 1, N_PROC_STATE + 1, width);
  table_t *tabela_proc_est_tempo = table_init(self->proc_qtd + 1, N_PROC_STATE + 1, width);

  table_fill(tabela_proc_geral, 0, 0, "PID");
  table_fill(tabela_proc_geral, 0, 1, "PREEMPCOES");
  table_fill(tabela_proc_geral, 0, 2, "RETURN");
  table_fill(tabela_proc_geral, 0, 3, "ANSWER");

  table_fill(tabela_proc_est_vezes, 0, 0, "PID");
  table_fill(tabela_proc_est_tempo, 0, 0, "PID");

  for (int i = 0; i < N_PROC_STATE; i++) {
    table_fill(tabela_proc_est_vezes, 0, i + 1, "%s", proc_state2string(i));
    table_fill(tabela_proc_est_tempo, 0, i + 1, "%s", proc_state2string(i));
  }

  for (int i = 0; i < self->proc_qtd; i++) {
    proc_t *proc = self->proc_table[i];

    proc_metrics_t proc_metricas_atual = proc_get_metrics(self->proc_table[i]);

    table_fill(tabela_proc_geral, i + 1, 0, "%d", proc_get_id(proc));
    table_fill(tabela_proc_geral, i + 1, 1, "%d", proc_metricas_atual.n_preempcoes);
    table_fill(tabela_proc_geral, i + 1, 2, "%d", proc_metricas_atual.t_return);
    table_fill(tabela_proc_geral, i + 1, 3, "%d", proc_metricas_atual.t_answer);

    table_fill(tabela_proc_est_vezes, i + 1, 0, "%d", proc_get_id(proc));
    table_fill(tabela_proc_est_tempo, i + 1, 0, "%d", proc_get_id(proc));

    for (int j = 0; j < N_PROC_STATE; j++) {
      table_fill(tabela_proc_est_vezes, i + 1, j + 1, "%d", proc_metricas_atual.states[j].n);
      table_fill(tabela_proc_est_tempo, i + 1, j + 1, "%d", proc_metricas_atual.states[j].total);
    }
  }

  // table_print(so_metrics_t);
  // table_print(irqs_table);
  // table_print(tabela_proc_geral);
  // table_print(tabela_proc_est_vezes);
  table_print(tabela_proc_est_tempo);

  table_free(so_metrics_t);
  table_free(tabela_proc_geral);
  table_free(tabela_proc_est_vezes);
  table_free(tabela_proc_est_tempo);
}

static int so_carrega_programa(so_t *self, char *nome_do_executavel)
{
  programa_t *prog = prog_cria(nome_do_executavel);
  if (prog == NULL) {
    console_printf("Erro na leitura do programa '%s'\n", nome_do_executavel);
    return -1;
  }

  int end_ini = prog_end_carga(prog);
  int end_fim = end_ini + prog_tamanho(prog);

  for (int end = end_ini; end < end_fim; end++) {
    if (mem_escreve(self->mem, end, prog_dado(prog, end)) != ERR_OK) {
      console_printf("Erro na carga da memoria, endereco %d\n", end);
      return -1;
    }
  }

  prog_destroi(prog);
  console_printf("SO: carga de '%s' em %d-%d", nome_do_executavel, end_ini, end_fim);
  return end_ini;
}

static bool copia_str_da_mem(int tam, char str[tam], mem_t *mem, int ender)
{
  for (int indice_str = 0; indice_str < tam; indice_str++) {
    int caractere;
    if (mem_le(mem, ender + indice_str, &caractere) != ERR_OK) {
      return false;
    }
    if (caractere < 0 || caractere > 255) {
      return false;
    }
    str[indice_str] = caractere;
    if (caractere == 0) {
      return true;
    }
  }
  return false;
}
