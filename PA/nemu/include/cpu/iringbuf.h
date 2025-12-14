#ifndef __MONITOR_IRINGBUF_H__
#define __MONITOR_IRINGBUF_H__

#include <common.h>
#include <cpu/decode.h>

void iringbuf_add(Decode *s);
void iringbuf_dump(vaddr_t crash_pc);

#endif
