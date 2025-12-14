#ifndef __CPU_FTRACE_H__
#define __CPU_FTRACE_H__

#include <stdint.h>
#include <stdbool.h>
#include <common.h>

#ifdef __cplusplus
extern "C" {
#endif

void ftrace_init(const char *path);
void ftrace_close(void);

bool ftrace_enabled(void);

const char *ftrace_lookup_func(vaddr_t addr, uint64_t *size_out);

void ftrace_on_jal(vaddr_t pc, vaddr_t target);

void ftrace_on_jalr(vaddr_t pc, int rd, int rs1, int32_t imm, vaddr_t target);

void ftrace_on_ret(vaddr_t pc);

void ftrace_push(void);
void ftrace_pop(void);

#ifdef __cplusplus
}
#endif

#endif