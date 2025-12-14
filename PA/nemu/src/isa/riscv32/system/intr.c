/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>
#include <cpu/cpu.h>
#include <common.h>

word_t isa_raise_intr(word_t NO, vaddr_t epc) {

#ifdef CONFIG_ETRACE
  bool is_intr = (NO >> (sizeof(word_t)*8 - 1)) & 1;

  log_write(
    "[eTrace] %s: mcause = 0x%08x, mepc = 0x%08x, mtvec = 0x%08x\n",
    is_intr ? "Interrupt" : "Exception",
    NO, epc, cpu.csr[1]  // mcause, mepc, mtvec
  );
#endif

  cpu.csr[3] = NO;   // mcause <- NO
  cpu.csr[2] = epc;  // mepc   <- epc
  return cpu.csr[1]; // mtvec
}


word_t isa_query_intr() {
  return INTR_EMPTY;
}
