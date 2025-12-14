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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

// this should be enough
static char buf[65536] = {};
static char calcbuf[65536 * 2] = {};
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char calc_code_buf[65536 * 2 + 128] = {};
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";

// 当前缓冲区位置
static int pos = 0;
static int calcpos = 0;

static uint32_t choose(uint32_t n) {
    return rand() % n;
}

static void gen(char c) {
    if (pos >= sizeof(buf) - 10) return; // 防止缓冲区溢出
    
    // 随机决定是否在字符前插入空格
    if (choose(2) == 0 && pos > 0 && buf[pos-1] != ' ') {
        buf[pos++] = ' ';
        calcbuf[calcpos++] = buf[pos - 1];
    }
    
    buf[pos++] = c;
    calcbuf[calcpos++] = c;

    // 随机决定是否在字符后插入空格
    if (choose(2) == 0) {
        buf[pos++] = ' ';
        calcbuf[calcpos++] = buf[pos - 1];
    }
}

// 生成随机数字
static void gen_num() {
    if (pos >= sizeof(buf) - 10) return;
    
    int len = choose(2) + 1;
    
    // 第一位不能是0，除非是单个0
    if (len == 1) {
        buf[pos++] = '0' + choose(10);
        calcbuf[calcpos++] = buf[pos - 1];
    } else {
        // 第一位不能是0
        buf[pos++] = '1' + choose(9);
        calcbuf[calcpos++] = buf[pos - 1];
        for (int i = 1; i < len; i++) {
            buf[pos++] = '0' + choose(10);
            calcbuf[calcpos++] = buf[pos - 1];
        }
    }
    
    calcbuf[calcpos++] = 'u';

    // 随机决定是否在数字后插入空格
    if (choose(2) == 0) {
        buf[pos++] = ' ';
        calcbuf[calcpos++] = buf[pos - 1];
    }
}

// 生成随机运算符
static void gen_rand_op() {
    if (pos >= sizeof(buf) - 2) return;
    
    switch (choose(4)) {
        case 0: gen('+'); break;
        case 1: gen('-'); break;
        case 2: gen('*'); break;
        case 3: gen('/'); break;
    }
}

static void gen_rand_expr_recursive(int depth) {
    if (depth > 10) { // 防止递归过深
        gen_num();
        return;
    }
    
    if (pos >= sizeof(buf) - 100) { // 缓冲区快满时生成简单表达式
        gen_num();
        return;
    }
    
    switch (choose(3)) {
        case 0: 
            gen_num(); 
            break;
        case 1: 
            gen('('); 
            gen_rand_expr_recursive(depth + 1); 
            gen(')'); 
            break;
        default: 
            gen_rand_expr_recursive(depth + 1);
            gen_rand_op();
            gen_rand_expr_recursive(depth + 1);
            break;
    }
}

static void gen_rand_expr() {
    pos = 0;
    buf[0] = '\0';
    calcpos = 0;
    calcbuf[0] = '\0';
    gen_rand_expr_recursive(0);
    buf[pos] = '\0'; // 确保字符串以null结尾
    calcbuf[calcpos] = '\0';

    // 确保表达式不以运算符结尾
    while (pos > 0 && (buf[pos-1] == '+' || buf[pos-1] == '-' || 
                       buf[pos-1] == '*' || buf[pos-1] == '/' ||
                       buf[pos-1] == ' ')) {
        pos--;
    }
    buf[pos] = '\0';

    while (calcpos > 0 && (calcbuf[calcpos-1] == '+' || calcbuf[calcpos-1] == '-' || 
                       calcbuf[calcpos-1] == '*' || calcbuf[calcpos-1] == '/' ||
                       calcbuf[calcpos-1] == ' ')) {
        calcpos--;
    }
    calcbuf[calcpos] = '\0';
}

int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 10;
  if (argc > 1) { 
    sscanf(argv[1], "%d", &loop); 
  }

  int i, datacount = 0;
  for (i = 0; i < loop; i ++) {
    gen_rand_expr();

    sprintf(code_buf, code_format, buf);
    sprintf(calc_code_buf, code_format, calcbuf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    FILE *calcfp = fopen("/tmp/.calccode.c", "w");
    assert(calcfp != NULL);
    fputs(calc_code_buf, calcfp);
    fclose(calcfp);


    int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
    if (ret != 0) continue;

    int calcret = system("gcc /tmp/.calccode.c -o /tmp/.calcexpr");
    if (calcret != 0) continue;

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    calcfp = popen("/tmp/.calcexpr", "r");
    assert(calcfp != NULL);

    int result;
    ret = fscanf(fp, "%d", &result);
    int exit_status = pclose(fp);
    if (WIFEXITED(exit_status)) {
        exit_status = WEXITSTATUS(exit_status);
    }

    int calcresult;
    calcret = fscanf(calcfp, "%d", &calcresult);
    int calc_exit_status = pclose(calcfp);
    if (WIFEXITED(calc_exit_status)) {
      calc_exit_status = WEXITSTATUS(calc_exit_status);
    }

    if (ret != 1 || exit_status != 0 || calcret != 1 || calc_exit_status != 0) {
        continue;
    }

    printf("%u %s\n", calcresult, buf);


    // fprintf(stderr, "buf: %s\n", buf);
    // fprintf(stderr, "calcbuf: %s\n", calcbuf);


    datacount++;
  }

  fprintf(stderr, "Real number of data: %d\n", datacount);
  return 0;
}