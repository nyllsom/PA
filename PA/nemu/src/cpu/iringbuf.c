#include <common.h>
#include <cpu/decode.h>
#include <cpu/cpu.h>
#include <cpu/iringbuf.h>

#ifdef CONFIG_IRINGBUF

#define IRINGBUF_SIZE 16

typedef struct {
  vaddr_t pc;
  char asmstr[128];
} IRingItem;

static IRingItem iringbuf[IRINGBUF_SIZE];
static int iringbuf_pos = 0;
static bool iringbuf_full = false;

void iringbuf_add(Decode *s) {
  strncpy(iringbuf[iringbuf_pos].asmstr, s->logbuf,
          sizeof(iringbuf[iringbuf_pos].asmstr));
  iringbuf[iringbuf_pos].asmstr[sizeof(iringbuf[iringbuf_pos].asmstr) - 1] = '\0';
  iringbuf[iringbuf_pos].pc = s->pc;
  iringbuf_pos = (iringbuf_pos + 1) % IRINGBUF_SIZE;
  if (iringbuf_pos == 0) iringbuf_full = true;
}

void iringbuf_dump(vaddr_t crash_pc) {
  printf("\n========== Instruction Ring Buffer ==========\n");
  int start = iringbuf_full ? iringbuf_pos : 0;
  int count = iringbuf_full ? IRINGBUF_SIZE : iringbuf_pos;
  for (int i = 0; i < count; i++) {
    int idx = (start + i) % IRINGBUF_SIZE;
    bool is_crash = (iringbuf[idx].pc == crash_pc);
    printf("%s " FMT_WORD ": %s\n",
           is_crash ? "-->" : "   ",
           iringbuf[idx].pc,
           iringbuf[idx].asmstr);
  }
  printf("=============================================\n");
}

#endif