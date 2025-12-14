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

#include <memory/host.h>
#include <memory/paddr.h>
#include <device/mmio.h>
#include <isa.h>
#include <../src/monitor/sdb/sdb.h> // TODO:这行可删
#include <common.h>

#if   defined(CONFIG_PMEM_MALLOC)
static uint8_t *pmem = NULL;
#else // CONFIG_PMEM_GARRAY
static uint8_t pmem[CONFIG_MSIZE] PG_ALIGN = {};
#endif

uint8_t* guest_to_host(paddr_t paddr) { return pmem + paddr - CONFIG_MBASE; }
paddr_t host_to_guest(uint8_t *haddr) { return haddr - pmem + CONFIG_MBASE; }

static word_t pmem_read(paddr_t addr, int len) {
  word_t ret = host_read(guest_to_host(addr), len);
  return ret;
}

static void pmem_write(paddr_t addr, int len, word_t data) {
  host_write(guest_to_host(addr), len, data);
}

static void out_of_bound(paddr_t addr) {
  panic("address = " FMT_PADDR " is out of bound of pmem [" FMT_PADDR ", " FMT_PADDR "] at pc = " FMT_WORD,
      addr, PMEM_LEFT, PMEM_RIGHT, cpu.pc);
}

void init_mem() {
#if   defined(CONFIG_PMEM_MALLOC)
  pmem = malloc(CONFIG_MSIZE);
  assert(pmem);
#endif
  IFDEF(CONFIG_MEM_RANDOM, memset(pmem, rand(), CONFIG_MSIZE));
  Log("physical memory area [" FMT_PADDR ", " FMT_PADDR "]", PMEM_LEFT, PMEM_RIGHT);
}

#ifdef CONFIG_MTRACE
static inline bool mtrace_in_range(paddr_t addr) {
  return addr >= (paddr_t)CONFIG_MTRACE_PADDR_START && addr < (paddr_t)CONFIG_MTRACE_PADDR_END;
}

static inline void mtrace_log(bool is_write, bool is_mmio, paddr_t addr, int len, word_t data) {
  if (!mtrace_in_range(addr)) return;

  log_write("[mtrace] pc=" FMT_WORD " %c%s addr=" FMT_PADDR " len=%d %s0x%0*" PRIx64 "\n",
            cpu.pc,
            is_write ? 'W' : 'R',
            is_mmio ? "(MMIO)" : "",
            addr, len,
            is_write ? "data=" : "=>",
            (int)(len * 2), (uint64_t)data);
}
#endif

word_t paddr_read(paddr_t addr, int len) {
  bool in = likely(in_pmem(addr));
  word_t ret = 0;

  if (in) {
    ret = pmem_read(addr, len);
#ifdef CONFIG_MTRACE
    mtrace_log(false, false, addr, len, ret);
#endif
    return ret;
  }

#ifdef CONFIG_DEVICE
  ret = mmio_read(addr, len);
#ifdef CONFIG_MTRACE
  if (CONFIG_MTRACE_MMIO) mtrace_log(false, true, addr, len, ret);
#endif
  return ret;
#endif

  out_of_bound(addr);
  return 0;
}

void paddr_write(paddr_t addr, int len, word_t data) {
  bool in = likely(in_pmem(addr));
#ifdef CONFIG_MTRACE
  mtrace_log(true, !in, addr, len, data);
#endif

  if (in) { pmem_write(addr, len, data); return; }

#ifdef CONFIG_DEVICE
  mmio_write(addr, len, data);
  return;
#endif

  out_of_bound(addr);
}
