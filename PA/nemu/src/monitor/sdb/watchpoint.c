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

#include "sdb.h"

#define NR_WP 32

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;

  char expr[128];
  word_t old_value;
  bool enabled;
} WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}

WP* new_wp() {
  if (free_ == NULL) {
    assert(0);
  }

  WP *wp = free_;
  free_ = free_ -> next;

  wp -> next = head;
  head = wp;

  return wp;
}

void free_wp(WP *wp) {
  if (wp == NULL) return;

  if (head == wp) {
    wp = head -> next;
  } else {
    WP *prev = head;
    while (prev != NULL && prev -> next != wp) {
      prev = prev -> next;
    }
    if (prev != NULL) {
      prev -> next = wp -> next;
    }
  }

  wp->next = free_;
  free_ = wp;

  wp -> enabled = false;
  wp -> expr[0] = '\0';
}

WP* find_wp_by_no(int no) {
  WP *wp = head;
  while (wp != NULL) {
    if (wp -> NO == no) {
      return wp;
    }
    wp = wp -> next;
  }
  return NULL;
}

bool set_watchpoint(char *exprssion) {
  bool success;
  word_t value = expr(exprssion, &success);
  if (!success) {
    printf("Invalid expression: %s\n", exprssion);
    return false;
  }

  WP *wp = new_wp();
  if (wp == NULL) {
    printf("No free watchpoint\n");
    return false;
  }

  strncpy(wp -> expr, exprssion, sizeof(wp -> expr) - 1);
  wp -> expr [sizeof(wp -> expr) - 1] = '\0';
  wp -> old_value = value;
  wp -> enabled = true;

  printf("Watchpoint %d: %s = 0x%08x\n", wp -> NO, wp -> expr, value);
  return true;
}

bool delete_watchpoint(int no) {
  WP *wp = find_wp_by_no(no);
  if (wp == NULL) {
    printf("No watchpoint's number is %d\n", no);
    return false;
  }

  free_wp(wp);
  printf("Delete watchpoint %d\n", no);
  return true;
}

void list_watchpoints() {
  if (head == NULL) {
    printf("No watchpoints\n");
    return;
  }

  printf("Num     Type           Disp Enb Address    What\n");
  WP *wp = head;
  while (wp != NULL) {
    if (wp->enabled) {
      printf("%-8dwatchpoint     keep y               %s = 0x%x\n", 
             wp->NO, wp->expr, wp->old_value);
    }
    wp = wp->next;
  }
}

void check_watchpoints() {
#ifdef CONFIG_WATCHPOINT
  WP *wp = head;
  while (wp != NULL) {
    if (wp -> enabled) {
      bool success;
      word_t new_value = expr(wp -> expr, &success);

      if (success && new_value != wp -> old_value) {
        printf("Watchpoint %d: %s\n\n", wp -> NO, wp -> expr);
        printf("Old value     = 0x%08x\n", wp -> old_value);
        printf("Current value = 0x%08x\n", new_value);
        wp -> old_value = new_value;

        nemu_state.state = NEMU_STOP;
        return;
      }
      wp->old_value = new_value;
    }
    wp = wp -> next;
  }
#endif
}