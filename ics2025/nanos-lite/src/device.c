#include <common.h>

#if defined(MULTIPROGRAM) && !defined(TIME_SHARING)
# define MULTIPROGRAM_YIELD() yield()
#else
# define MULTIPROGRAM_YIELD()
#endif

#define NAME(key) \
  [AM_KEY_##key] = #key,

static const char *keyname[256] __attribute__((used)) = {
  [AM_KEY_NONE] = "NONE",
  AM_KEYS(NAME)
};

size_t serial_write(const void *buf, size_t offset, size_t len) {
  const char *s = buf;
  for (size_t i = 0; i < len; i++) {
    putch(s[i]);
  }
  return len;
}

size_t events_read(void *buf, size_t offset, size_t len) {
  AM_INPUT_KEYBRD_T kbd = io_read(AM_INPUT_KEYBRD);
  if (kbd.keycode == AM_KEY_NONE) return 0;

  char event[32]; 
  int n = snprintf(event, sizeof(event),
                   "%s %s",
                   kbd.keydown ? "kd" : "ku",
                   keyname[kbd.keycode]);

  if (n < 0) return 0;

  size_t real = (size_t)n < len ? (size_t)n : len;
  memcpy(buf, event, real);

  return real;
}

static uint32_t screen_w = 0, screen_h = 0;

size_t dispinfo_read(void *buf, size_t offset, size_t len) {
  AM_GPU_CONFIG_T cfg = io_read(AM_GPU_CONFIG);
  screen_w = cfg.width;
  screen_h = cfg.height;
  return snprintf((char *)buf, len, "WIDTH : %d\nHEIGHT : %d\n", screen_w, screen_h);
}

size_t fb_write(const void *buf, size_t offset, size_t len) {
  if (screen_w == 0 || screen_h == 0) {
    AM_GPU_CONFIG_T cfg = io_read(AM_GPU_CONFIG);
    screen_w = cfg.width;
    screen_h = cfg.height;
  }

  if ((offset & 0x3) != 0) return 0;
  len &= ~0x3; 
  if (len == 0) return 0;

  const uint8_t *p = (const uint8_t *)buf;
  size_t written = 0;

  const size_t row_bytes = (size_t)screen_w * 4;

  while (written < len) {
    size_t cur_off = offset + written;

    uint32_t y = cur_off / row_bytes;
    uint32_t x = (cur_off % row_bytes) / 4;

    if (y >= screen_h) break;

    size_t bytes_left_in_row = row_bytes - (cur_off % row_bytes);
    size_t chunk = (len - written < bytes_left_in_row) ? (len - written) : bytes_left_in_row;

    uint32_t w = chunk / 4;
    if (w == 0) break;

    io_write(AM_GPU_FBDRAW,
             x, y,
             (uint32_t *)(p + written),
             w, 1,
             true /* sync */);

    written += chunk;
  }

  return written;
}

void init_device() {
  Log("Initializing devices...");
  ioe_init();
}
