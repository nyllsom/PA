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
#include <readline/readline.h>
#include <readline/history.h>
#include <stdint.h>
#include <memory/vaddr.h>
#include "sdb.h"

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_q(char *args) {
  nemu_state.state = NEMU_QUIT;
  return -1;
}
 
static int cmd_si(char *args) {
  int steps = 1;
  
  if (args != NULL) {
    steps = atoi(args);
    if (steps <= 0) {
      printf("Invalid step count: %d\n", steps);
      return 0;
    }
  }

  cpu_exec(steps);
  return 0;
}

static int cmd_info(char *args) {
  if (args == NULL) {
    printf("Usage: info r - print registers\n");
    printf("       info w - list watchpoints\n");
    return 0;
  }

  char *subcmd = strtok(args, " ");
  if (strcmp(subcmd, "r") == 0) {
    isa_reg_display();
  } else if (strcmp(subcmd, "w") == 0) {
    list_watchpoints();
  } else {
    printf("Usage: info r - print registers\n");
    printf("       info w - list watchpoints\n");
  }

  return 0;
}

static int cmd_x(char *args) {
  if (args == NULL) {
    printf("Please enter parameters!\n");
    printf("Usage: x N EXPR - scan memory\n");
    return 0;
  }

  char *count_str = strtok(args, " ");
  char *addr_str = strtok(NULL, " ");

  if (count_str  == NULL || addr_str == NULL) {
    printf("Invalid parameters!\n");
    printf("Usage: x N EXPR - scan memory\n");
    return 0;
  }

  int count = atoi(count_str);

  if (count <= 0) {
    printf("Invalid count!\n");
    printf("Usage: x N EXPR - scan memory\n");
    return 0;
  }
  bool success = true;
  word_t addr = expr(addr_str, &success);
  if (!success) {
    printf("Invalid expression: %s\n", addr_str);
    return 0;
  }

  for (int i = 0; i < count; i++) {
    printf("0x%08x: ", addr + i * 4);
    word_t data = vaddr_read(addr + i * 4, 4);
    printf("%08x", data);
    printf("\n");
  }

  return 0;
}

static int test_expressions(const char *filename) {
  char filepath[1000] = "/home/nyllsom/Documents/Repository/ics2025/nemu/src/monitor/sdb/";
  strcat(filepath, filename);
  printf("filepath: %s\n", filepath);
  FILE *file = fopen(filepath, "r");
  char badpath[1000] = "/home/nyllsom/Documents/Repository/ics2025/nemu/src/monitor/sdb/bad";
  FILE *bad = fopen(badpath, "w");
  fprintf(bad, "1 1\n");
  bad = fopen(badpath, "a");

  if (!file) {
      printf("Cannot open test file: %s\n", filename);
      return 1;
  }

  char line[1024];
  int total = 0, passed = 0;
  
  while (fgets(line, sizeof(line), file)) {
    // 跳过空行
    if (strlen(line) <= 1) continue;
    
    // 解析行：格式为 "结果 表达式"
    char *space = strchr(line, ' ');
    if (!space) {
        printf("Invalid line format: %s", line);
        continue;
    }
    
    // 分割结果和表达式
    *space = '\0';
    unsigned expected_result = (unsigned)atoi(line);
    char *expression = space + 1;
    
    // 去除表达式末尾的换行符
    char *newline = strchr(expression, '\n');
    if (newline) *newline = '\0';
    
    total++;
    
    // 调用 expr 函数求值
    bool success;
    word_t result = expr(expression, &success);
    
    if (!success) {
      printf("FAIL: Expression evaluation failed: %s\n", expression);
      continue;
    }
    
    if ((unsigned)result == expected_result) {
      passed++;
      printf("PASS: %s = %u\n", expression, expected_result);
    } else {
      printf("FAIL: %s\n", expression);
      printf("  Expected: %u, Got: %u\n", expected_result, (unsigned)result);
      fprintf(bad, "%u %s\n", expected_result, expression);
    }
  }
  
  fclose(file);
  fclose(bad);
  printf("\nTest Summary: %d/%d passed (%.1f%%)\n", 
          passed, total, (float)passed/total*100);
  
  return 0;
}

static int cmd_testexpr(char *args) {
  if (args == NULL) {
    printf("Usage: testexpr FILENAME\n");
    return 0;
  }

  char *filename = strtok(args, " ");
  if (test_expressions(filename)) {
    printf("Please make sure that data file is in the same directory as sdb.c (/nemu/src/monitor/sdb).\n");
  }
  return 0;
}

static int cmd_w(char *args) {
  if (args == NULL) {
    printf("Usage: w EXPRESSION");
    return 0;
  }
  return set_watchpoint(args);
}

static int cmd_d(char *args) {
  if (args == NULL) {
    printf("Usage: d NO\n");
    return 0;
  }
  int no = atoi(args);
  return delete_watchpoint(no);
}

static int cmd_p(char *args) {
  if (args == NULL) {
    printf("Usage: p EXPR\n");
    return 0;
  }

  bool success = true;
  word_t result = expr(args, &success);
  if (!success) {
    fprintf(stderr, "Invalid input!\n");
    return 0;
  }
  
  printf("%u\n", result);
  return 0;
}

static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si", "Single step execution (si [N])", cmd_si },
  { "info", "Print program information\n\t(info r - print registers)\n\tinfo w - list watchpoints", cmd_info },
  { "x", "Scan memory (x N EXPR)", cmd_x },
  { "testexpr", "Test experssion computation", cmd_testexpr },
  { "w", "Set watchpoint", cmd_w },
  { "d", "Delete watchpoint", cmd_d },
  { "p", "Evaluating a simple expression", cmd_p },
};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
