#include <common.h>

#ifdef CONFIG_FTRACE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <elf.h>
#include <stdarg.h>
#include <inttypes.h>
#include <errno.h>
#include "common.h"      
#include "cpu/ftrace.h"

#ifndef CONFIG_FTRACE_INDENT
#define CONFIG_FTRACE_INDENT 2
#endif

typedef struct {
  char    *name;
  vaddr_t  addr;
  uint64_t size;
} FuncSym;

static FuncSym *g_syms = NULL;
static size_t   g_nr   = 0;
static int      g_depth = 0;
static int      g_enabled = 0;

static char *g_strtab = NULL;
static size_t g_strtab_sz = 0;

static inline void ftrace_log(const char *fmt, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n < 0) return;
  if (n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;

  // 写入 NEMU log
  log_write("%.*s", n, buf);

#ifdef CONFIG_FTRACE_LOG_TO_STDOUT
  // 可选：同时输出到 stdout
  fwrite(buf, 1, n, stdout);
  fflush(stdout);
#endif
}

static int cmp_by_addr(const void *a, const void *b) {
  const FuncSym *x = (const FuncSym *)a;
  const FuncSym *y = (const FuncSym *)b;
  if (x->addr < y->addr) return -1;
  if (x->addr > y->addr) return 1;
  return 0;
}

static const char *lookup_name(vaddr_t addr, uint64_t *size_out) {
  if (!g_enabled || g_nr == 0) {
    if (size_out) *size_out = 0;
    return "???";
  }
  // 二分找最后一个 <= addr 的符号，然后检查是否在区间内
  size_t l = 0, r = g_nr;
  while (l < r) {
    size_t m = (l + r) >> 1;
    if (g_syms[m].addr <= addr) l = m + 1;
    else r = m;
  }
  if (r == 0) { if (size_out) *size_out = 0; return "???"; }
  const FuncSym *s = &g_syms[r-1];
  if (addr >= s->addr && addr < s->addr + s->size && s->name) {
    if (size_out) *size_out = s->size;
    return s->name;
  }
  if (size_out) *size_out = 0;
  return "???";
}

const char *ftrace_lookup_func(vaddr_t addr, uint64_t *size_out) {
  return lookup_name(addr, size_out);
}

bool ftrace_enabled(void) { return g_enabled != 0; }

static void free_all(void) {
  if (g_syms) {
    for (size_t i = 0; i < g_nr; i++) free(g_syms[i].name);
    free(g_syms); g_syms = NULL; g_nr = 0;
  }
  if (g_strtab) { free(g_strtab); g_strtab = NULL; g_strtab_sz = 0; }
}

void ftrace_close(void) { free_all(); g_enabled = 0; }

static int load_elf32(FILE *fp, const Elf32_Ehdr *eh) {
  // 读节区字符串表（用于找 .symtab 名称不必须，但有用）
  // 实际上直接遍历找到 SHT_SYMTAB 即可
  // 先读所有 section headers
  if (eh->e_shentsize != sizeof(Elf32_Shdr)) {
    // 某些工具链也可能不同步，但常规应一致
  }

  // 找 symtab 和其 link 的 strtab
  Elf32_Shdr *shdrs = (Elf32_Shdr*)malloc(eh->e_shnum * sizeof(Elf32_Shdr));
  fseek(fp, eh->e_shoff, SEEK_SET);
  if (fread(shdrs, sizeof(Elf32_Shdr), eh->e_shnum, fp) != eh->e_shnum) {
    log_write("[ftrace] read section headers failed\n");
    free(shdrs);
    return -1;
  }


  int sym_idx = -1;
  int str_idx = -1;

  for (int i = 0; i < eh->e_shnum; i++) {
    if (shdrs[i].sh_type == SHT_SYMTAB) {
      sym_idx = i;
      str_idx = shdrs[i].sh_link; // 规范规定 link 指向字符串表
      break;
    }
  }
  if (sym_idx < 0 || str_idx < 0) { free(shdrs); return -1; }

  // 读字符串表
  {
    Elf32_Shdr *str = &shdrs[str_idx];
    g_strtab_sz = str->sh_size;
    g_strtab = (char*)malloc(g_strtab_sz);
    fseek(fp, str->sh_offset, SEEK_SET);
    if (fread(g_strtab, 1, g_strtab_sz, fp) != g_strtab_sz) {
      log_write("[ftrace] read section headers failed\n");
      free(shdrs);
      return -1;
    }
  }

  // 读符号表
  Elf32_Shdr *sym = &shdrs[sym_idx];
  size_t nsyms = sym->sh_size / sizeof(Elf32_Sym);
  Elf32_Sym *syms = (Elf32_Sym*)malloc(sym->sh_size);
  fseek(fp, sym->sh_offset, SEEK_SET);
  if (fread(syms, sizeof(Elf32_Sym), nsyms, fp) != nsyms) {
    log_write("[ftrace] read section headers failed\n");
    free(shdrs);
    return -1;
  }


  // 统计 STT_FUNC
  size_t cnt = 0;
  for (size_t i = 0; i < nsyms; i++) {
    if (ELF32_ST_TYPE(syms[i].st_info) == STT_FUNC && syms[i].st_size > 0 && syms[i].st_name < g_strtab_sz) {
      cnt++;
    }
  }
  g_syms = (FuncSym*)calloc(cnt, sizeof(FuncSym));
  g_nr = 0;
  for (size_t i = 0; i < nsyms; i++) {
    if (ELF32_ST_TYPE(syms[i].st_info) == STT_FUNC && syms[i].st_size > 0 && syms[i].st_name < g_strtab_sz) {
      g_syms[g_nr].addr = (vaddr_t)syms[i].st_value;
      g_syms[g_nr].size = (uint64_t)syms[i].st_size;
      const char *nm = &g_strtab[syms[i].st_name];
      g_syms[g_nr].name = strdup(nm ? nm : "???");
      g_nr++;
    }
  }
  free(syms);
  free(shdrs);

  qsort(g_syms, g_nr, sizeof(FuncSym), cmp_by_addr);
  return 0;
}

static int load_elf64(FILE *fp, const Elf64_Ehdr *eh) {
  Elf64_Shdr *shdrs = (Elf64_Shdr*)malloc(eh->e_shnum * sizeof(Elf64_Shdr));
  fseek(fp, eh->e_shoff, SEEK_SET);
  if (fread(shdrs, sizeof(Elf64_Shdr), eh->e_shnum, fp) != eh->e_shnum) {
    log_write("[ftrace] read section headers failed\n");
    free(shdrs);
    return -1;
  }

  int sym_idx = -1;
  int str_idx = -1;
  for (int i = 0; i < eh->e_shnum; i++) {
    if (shdrs[i].sh_type == SHT_SYMTAB) {
      sym_idx = i;
      str_idx = shdrs[i].sh_link;
      break;
    }
  }
  if (sym_idx < 0 || str_idx < 0) { free(shdrs); return -1; }

  // strtab
  {
    Elf64_Shdr *str = &shdrs[str_idx];
    g_strtab_sz = str->sh_size;
    g_strtab = (char*)malloc(g_strtab_sz);
    fseek(fp, str->sh_offset, SEEK_SET);
    if (fread(g_strtab, 1, g_strtab_sz, fp) != g_strtab_sz) {
      log_write("[ftrace] read section headers failed\n");
      free(shdrs);
      return -1;
    }
  }

  // symtab
  Elf64_Shdr *sym = &shdrs[sym_idx];
  size_t nsyms = sym->sh_size / sizeof(Elf64_Sym);
  Elf64_Sym *syms = (Elf64_Sym*)malloc(sym->sh_size);
  fseek(fp, sym->sh_offset, SEEK_SET);
  if (fread(syms, sizeof(Elf64_Sym), nsyms, fp) != nsyms) {
    log_write("[ftrace] read section headers failed\n");
    free(shdrs);
    return -1;
  }

  size_t cnt = 0;
  for (size_t i = 0; i < nsyms; i++) {
    if (ELF64_ST_TYPE(syms[i].st_info) == STT_FUNC && syms[i].st_size > 0 && syms[i].st_name < g_strtab_sz) {
      cnt++;
    }
  }
  g_syms = (FuncSym*)calloc(cnt, sizeof(FuncSym));
  g_nr = 0;
  for (size_t i = 0; i < nsyms; i++) {
    if (ELF64_ST_TYPE(syms[i].st_info) == STT_FUNC && syms[i].st_size > 0 && syms[i].st_name < g_strtab_sz) {
      g_syms[g_nr].addr = (vaddr_t)syms[i].st_value;
      g_syms[g_nr].size = (uint64_t)syms[i].st_size;
      const char *nm = &g_strtab[syms[i].st_name];
      g_syms[g_nr].name = strdup(nm ? nm : "???");
      g_nr++;
    }
  }
  free(syms);
  free(shdrs);

  qsort(g_syms, g_nr, sizeof(FuncSym), cmp_by_addr);
  return 0;
}

void ftrace_init(const char *path) {
  free_all();
  g_depth = 0;
  g_enabled = 0;

  if (path == NULL || strcmp(path, "none") == 0 || strlen(path) == 0) {
#ifdef CONFIG_FTRACE_ELF_PATH
    // 如果编译期 Kconfig 里配置了默认路径，可从宏里拿
#endif
    return;
  }

  FILE *fp = fopen(path, "rb");
  if (!fp) {
    log_write("[ftrace] cannot open ELF: %s (%s)\n", path, strerror(errno));
    return;
  }

  unsigned char ident[EI_NIDENT];
  if (fread(ident, 1, EI_NIDENT, fp) != EI_NIDENT) {
    fclose(fp);
    log_write("[ftrace] read ELF ident failed\n");
    return;
  }
  if (memcmp(ident, ELFMAG, SELFMAG) != 0) {
    fclose(fp);
    log_write("[ftrace] not an ELF file: %s\n", path);
    return;
  }
  // 回到文件头，分别读 ehdr32/ehdr64
  fseek(fp, 0, SEEK_SET);

  int ret = -1;
  if (ident[EI_CLASS] == ELFCLASS32) {
    Elf32_Ehdr eh;
    if (fread(&eh, sizeof(eh), 1, fp) != 1) {
      log_write("[ftrace] failed to read ELF32 header: %s\n", path);
      fclose(fp);
      return;
    }
    ret = load_elf32(fp, &eh);
  } else if (ident[EI_CLASS] == ELFCLASS64) {
    Elf64_Ehdr eh;
    if (fread(&eh, sizeof(eh), 1, fp) != 1) {
      log_write("[ftrace] failed to read ELF64 header: %s\n", path);
      fclose(fp);
      return;
    }
    ret = load_elf64(fp, &eh);
  } else {
    log_write("[ftrace] unsupported ELF class\n");
  }
  fclose(fp);

  if (ret == 0 && g_nr > 0) {
    g_enabled = 1;
    log_write("[ftrace] loaded %zu function symbols from %s\n", g_nr, path);
  } else {
    log_write("[ftrace] no function symbols found in %s\n", path);
  }
}

static inline void put_indent(int depth) {
  int spaces = CONFIG_FTRACE_INDENT * (depth < 0 ? 0 : depth);
  while (spaces-- > 0) ftrace_log(" ");
}

void ftrace_push(void) { g_depth++; }
void ftrace_pop(void)  { if (g_depth > 0) g_depth--; }

static void on_call(vaddr_t pc, vaddr_t target) {
  const char *callee = lookup_name(target, NULL);
  put_indent(g_depth);
  ftrace_log("0x%0*" PRIx64 ": call [%s@0x%0*" PRIx64 "]\n",
             (int)(sizeof(vaddr_t) * 2), (uint64_t)pc,
             callee,
             (int)(sizeof(vaddr_t) * 2), (uint64_t)target);
  ftrace_push();
}

static void on_ret(vaddr_t pc) {
  ftrace_pop();
  const char *cur = lookup_name(pc, NULL);
  put_indent(g_depth);
  ftrace_log("0x%0*" PRIx64 ": ret  [%s]\n",
             (int)(sizeof(vaddr_t) * 2), (uint64_t)pc, cur);
}

void ftrace_on_jal(vaddr_t pc, vaddr_t target) {
  if (!g_enabled) return;
  on_call(pc, target);
}

void ftrace_on_ret(vaddr_t pc) {
  if (!g_enabled) return;
  on_ret(pc);
}

void ftrace_on_jalr(vaddr_t pc, int rd, int rs1, int32_t imm, vaddr_t target) {
  if (!g_enabled) return;
  // RISC-V 返回: jalr x0, x1, 0
  if (rd == 0 && rs1 == 1 && imm == 0) {
    on_ret(pc);
    return;
  }
  // 典型调用：rd == x1(ra)
  if (rd == 1) {
    on_call(pc, target);
    return;
  }
  // 尾调用：rd == x0 且 rs1 != x1
  if (rd == 0 && rs1 != 1) {
    on_call(pc, target);
    return;
  }
  // 其它 jalr 多为跳转表/间接跳转，忽略
}

#endif