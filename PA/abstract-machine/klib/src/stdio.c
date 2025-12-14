#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

static void reverse(char * str, int len) {
  int i = 0, j = len - 1;
  while (i < j) {
    char tmp = str[i];
    str[i] = str[j];
    str[j] = tmp;
    i++;
    j--;
  }
}

static int itoa_signed(int val, char *buf, int base, bool upper) {
  bool neg = false;
  if (val < 0) {
    neg = true;
    val = -val;
  }

  const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
  int i = 0;
  do {
    buf[i++] = digits[val % base];
    val /= base;
  } while (val > 0);

  if (neg) buf[i++] = '-';
  buf[i] = '\0';
  reverse(buf, i);
  return i;
}

static int itoa_unsigned(uint32_t val, char *buf, int base, bool upper) {
  const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
  int i = 0;
  do {
    buf[i++] = digits[val % base];
    val /= base;
  } while (val > 0);

  buf[i] = '\0';
  reverse(buf, i);
  return i;
}

static int itoa_ptr(uintptr_t val, char *buf) {
  const char *digits = "0123456789abcdef";
  int i = 0;

  // 输出 "0x"
  buf[i++] = '0';
  buf[i++] = 'x';

  int start = i;

  do {
    buf[i++] = digits[val % 16];
    val /= 16;
  } while (val > 0);

  reverse(buf + start, i - start);

  buf[i] = '\0';
  return i;
}

int vsprintf(char *out, const char *fmt, va_list ap) {
  char *p = out;

  for ( ; *fmt; fmt++) {
    if (*fmt != '%') { *p++ = *fmt; continue; }

    fmt++;
    if (*fmt == '\0') break;

    switch (*fmt) {
      case 'd': {
        int val = va_arg(ap, int);
        char buf[32];
        int len = itoa_signed(val, buf, 10, false);
        for (int i = 0; i < len; i++) *p++ = buf[i];
        break;
      }
      case 'u': {
        unsigned int val = va_arg(ap, unsigned int);
        char buf[32];
        int len = itoa_unsigned(val, buf, 10, false);
        for (int i = 0; i < len; i++) *p++ = buf[i];
        break;
      }
      case 'x': {
        unsigned int val = va_arg(ap, unsigned int);
        char buf[32];
        int len = itoa_unsigned(val, buf, 16, false);
        for (int i = 0; i < len; i++) *p++ = buf[i];
        break;
      }
      case 'X': {
        unsigned int val = va_arg(ap, unsigned int);
        char buf[32];
        int len = itoa_unsigned(val, buf, 16, true);
        for (int i = 0; i < len; i++) *p++ = buf[i];
        break;
      }
      case 'c': {
        char ch = (char)va_arg(ap, int); 
        /*  
          default argument promotion: 
            char/signed char/unsigend char/short -> int
            less than int -> int
            float -> double
            int/double/pointers remain unchanged
        */
        *p++ = ch;
        break;
      }
      case 's': {
        const char *s = va_arg(ap, const char *);
        while (*s) *p++ = *s++;
        break;
      }
      case 'p': {
        void *ptr = va_arg(ap, void *);
        char buf2[32];
        int len = itoa_ptr((uintptr_t)ptr, buf2);
        for (int i = 0; i < len; i++) *p++ = buf2[i];
        break;
      }
      case '%': {
        *p++ = '%';
        break;
      }
      default: { // unknown format, output directly
        *p++ = '%';
        *p++ = *fmt;
        break;
      }
    }
  }

  *p = '\0';
  return p - out;
}

#define PRINTF_MAXLEN 1024

static char buf[PRINTF_MAXLEN];

int printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);  
  int ret = vsprintf(buf, fmt, ap);
  va_end(ap);
  putstr(buf);
  return ret;
}

int sprintf(char *out, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = vsprintf(out, fmt, ap);
  va_end(ap);
  return ret;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  if (n == 0) {
    char temp[PRINTF_MAXLEN];
    va_list ap_copy;
    va_copy(ap_copy, ap);
    int len = vsprintf(temp, fmt, ap_copy);
    va_end(ap_copy);
    return len;
  }

  char temp[PRINTF_MAXLEN];
  va_list ap_copy;
  va_copy(ap_copy, ap);

  int len = vsprintf(temp, fmt, ap_copy);
  va_end(ap_copy);

  size_t copy_len = (len < n - 1) ? len : (n - 1);
  for (size_t i = 0; i < copy_len; i++) {
    out[i] = temp[i];
  }

  out[copy_len] = '\0';
  return len;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = vsnprintf(out, n, fmt, ap);
  va_end(ap);
  return ret;
}

#endif
