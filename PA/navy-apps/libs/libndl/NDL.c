#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

static int evtdev = -1;
static int fbdev = -1;
static int screen_w = 0, screen_h = 0;
static int canvas_w = 0, canvas_h = 0;
static int canvas_x = 0, canvas_y = 0;

uint32_t NDL_GetTicks() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

extern int open (const char *file, int flags, ...);

int NDL_PollEvent(char *buf, int len) {
  int fd = open("/dev/events", 0);
  int ret = read(fd, buf, len);
  return ret > 0 ? 1 : 0;
}

// void NDL_OpenCanvas(int *w, int *h) {
//   if (getenv("NWM_APP")) {
//     int fbctl = 4;
//     fbdev = 5;
//     screen_w = *w; screen_h = *h;
//     char buf[64];
//     int len = sprintf(buf, "%d %d", screen_w, screen_h);
//     // let NWM resize the window and create the frame buffer
//     write(fbctl, buf, len);
//     while (1) {
//       // 3 = evtdev
//       int nread = read(3, buf, sizeof(buf) - 1);
//       if (nread <= 0) continue;
//       buf[nread] = '\0';
//       if (strcmp(buf, "mmap ok") == 0) break;
//     }
//     close(fbctl);
//   }
// }

void NDL_OpenCanvas(int *w, int *h) {
  if(*w == 0 && *h == 0) {
    *w = screen_w;
    *h = screen_h;
  } 
  canvas_w = *w;
  canvas_h = *h;
}

void NDL_DrawRect(uint32_t *pixels, int x, int y, int w, int h) {
  if (x < 0) { pixels += (-x); w += x; x = 0; }
  if (y < 0) { pixels += (-y) * w; h += y; y = 0; }
  if (x + w > canvas_w) w = canvas_w - x;
  if (y + h > canvas_h) h = canvas_h - y;
  if (w <= 0 || h <= 0) return;

  int fd = open("/dev/fb", 0, 0);

  int sx = canvas_x + x;
  int sy = canvas_y + y;

  for (int i = 0; i < h; i++) {
    off_t off = ((off_t)(sy + i) * screen_w + sx) * 4;
    lseek(fd, off, SEEK_SET);
    write(fd, pixels + (size_t)i * w, (size_t)w * 4);
  }

  close(fd);
}

void NDL_OpenAudio(int freq, int channels, int samples) {
}

void NDL_CloseAudio() {
}

int NDL_PlayAudio(void *buf, int len) {
  return 0;
}

int NDL_QueryAudio() {
  return 0;
}


int NDL_Init(uint32_t flags) {
  (void)flags;
  char buf[64] = {0};
  int fd = open("/proc/dispinfo", 0, 0);
  int n = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n <= 0) return -1;

  sscanf(buf, "WIDTH : %d\nHEIGHT : %d\n", &screen_w, &screen_h);
  if (getenv("NWM_APP")) {
    evtdev = 3;
  }
  return 0;
}

void NDL_Quit() {
}
