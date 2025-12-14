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

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>
#include <memory/vaddr.h>
#include <debug.h>

enum {
  TK_NOTYPE = 256, 
  TK_EQ, TK_NEQ, TK_AND, TK_OR,
  TK_NUM, TK_HEX, 
  TK_REG,
  TK_NEG, 
  TK_DEREF
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {
  {" +", TK_NOTYPE},            // spaces
  {"\\+", '+'},                 // plus
  {"-", '-'},                   // minus
  {"\\*", '*'},                 // times
  {"/", '/'},                   // divide
  {"\\(", '('},                 // left parenthesis
  {"\\)", ')'},                 // right parenthesis                     
  {"==", TK_EQ},                // equal
  {"!=", TK_NEQ},               // not equal
  {"&&", TK_AND},               // logic and
  {"\\|\\|", TK_OR},           // logic or
  {"0[xX][0-9a-fA-F]+", TK_HEX},  // hexadecimal
  {"[0-9]+", TK_NUM},          // decimal
  {"\\$[\\$0-9a-zA-Z]+", TK_REG}     // register
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[8192] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        // char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        // Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
        //    i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        switch (rules[i].token_type) {
          case TK_NOTYPE: 
            break;
          default: 
            tokens[nr_token].type = rules[i].token_type;
            int len = pmatch.rm_eo;
            if (len >= sizeof(tokens[nr_token].str)) len = sizeof(tokens[nr_token].str) - 1;
            strncpy(tokens[nr_token].str, e + position - substr_len, len);
            tokens[nr_token].str[len] = '\0';
            nr_token++;
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  for (int i = 0; i < nr_token; i++) {
    if (tokens[i].type == '*' &&
      (i == 0 || (tokens[i - 1].type != TK_NUM && tokens[i - 1].type != TK_HEX
        && tokens[i - 1].type != TK_REG && tokens[i - 1].type != ')'))) {
      tokens[i].type = TK_DEREF;
    }
    if (tokens[i].type == '-' &&
      (i == 0 || (tokens[i - 1].type != TK_NUM && tokens[i - 1].type != TK_HEX
        && tokens[i - 1].type != TK_REG && tokens[i - 1].type != ')'))) {
      tokens[i].type = TK_NEG;    
    }
  }

  return true;
}

word_t eval(int p, int q, bool *success);

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {

    // Log("Fail to make token");

    *success = false;
    return 0;
  }

  // DEBUG output
  // Log("Get %d tokens\n", nr_token);

  // for (int i = 0; i < nr_token; i++) {
  //   printf("%s type:%d\n", tokens[i].str, tokens[i].type);
  // }

  *success = true;
  return eval(0, nr_token - 1, success);
}

static bool check_parentheses(int p, int q) {
  if (tokens[p].type != '(' || tokens[q].type != ')') return false;
  int depth = 0;
  for (int i = p; i <= q; i++) {
    if (tokens[i].type == '(') depth++;
    else if (tokens[i].type == ')') depth--;
    if (depth == 0 && i < q) return false;
  }
  return depth == 0;
}

static int precedence(int type) {
  switch (type) {
    case TK_OR: return 1;
    case TK_AND: return 2;
    case TK_EQ: case TK_NEQ: return 3;
    case '+': case '-': return 4;
    case '*': case '/': return 5;
    case TK_NEG: case TK_DEREF: return 6;
    default: return 0;
  }
}

static int find_main_op(int p, int q) {
  int op = -1, min_pri = 100;
  int depth = 0;
  for (int i = p; i <= q; i++) {
    int type = tokens[i].type;
    if (type == '(') depth++;
    else if (type == ')') depth--;
    else if (depth == 0 && precedence(type) > 0) {
      int pri = precedence(type);
      if (pri <= min_pri) {
        min_pri = pri;
        op = i; 
      }
    }
  }
  return op;
}

word_t eval(int p, int q, bool *success) {
  if (p > q) {
    *success = false;
    return 0;
  }
  else if (p == q) {
    if (tokens[p].type == TK_NUM) return atoi(tokens[p].str);
    if (tokens[p].type == TK_HEX) return strtol(tokens[p].str, NULL, 16); 
    if (tokens[p].type == TK_REG) {
      bool ok = true;
      const char *regname = tokens[p].str + 1;

      // Log("regname: %s\n", regname);

      word_t val = isa_reg_str2val(regname, &ok);
      if (!ok) {
        *success = false;
        return 0;
      }
      return val;
      // TODO: Here the register names are like 'a0' 'z1' or something, not 'eax' or stuff.
    } 
  }
  else if (check_parentheses(p, q) == true) {
    return eval(p + 1, q - 1, success);
  }

  int op = find_main_op(p, q);
  if (op < 0) {
    *success = false;
    return 0;
  }

  if (tokens[op].type == TK_NEG) 
    return -eval(op + 1, q, success);
  if (tokens[op].type == TK_DEREF) 
    return vaddr_read(eval(op + 1, q, success), 4);

  word_t val1 = eval(p, op - 1, success);
  word_t val2 = eval(op + 1, q, success);

  switch(tokens[op].type) {
    case '+':    return val1 + val2;
    case '-':    return val1 - val2;
    case '*':    return val1 * val2;
    case '/':    if (val2) {
                  return val2;
                 } else {
                  Log("Divisor is zero. Output zero as default.");
                  return 0;
                 } 
    case TK_EQ:  return val1 == val2;
    case TK_NEQ: return val1 != val2;
    case TK_AND: return val1 && val2;
    case TK_OR:  return val1 || val2;
    default: panic("unknown operator");
  }
}
