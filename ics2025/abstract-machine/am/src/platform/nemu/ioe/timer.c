#include <am.h>
#include <nemu.h>
#include <klib.h>

static uint64_t boot_time_us = 0;

static inline uint64_t read_rtc_us() {
  uint32_t hi1, lo, hi2;
  do {
    hi1 = inl(RTC_ADDR + 4);
    lo  = inl(RTC_ADDR + 0);
    hi2 = inl(RTC_ADDR + 4);
  } while (hi1 != hi2);
  return ((uint64_t)hi1 << 32) | lo;
}

void __am_timer_init() {
  boot_time_us = read_rtc_us();
}

void __am_timer_uptime(AM_TIMER_UPTIME_T *uptime) {
  uint64_t now = read_rtc_us();
  uptime->us = now - boot_time_us;
}


void __am_timer_rtc(AM_TIMER_RTC_T *rtc) {
  rtc->second = 0;
  rtc->minute = 0;
  rtc->hour   = 0;
  rtc->day    = 0;
  rtc->month  = 0;
  rtc->year   = 1900;
}
