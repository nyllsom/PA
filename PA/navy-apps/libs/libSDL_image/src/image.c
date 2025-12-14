#define SDL_malloc  malloc
#define SDL_free    free
#define SDL_realloc realloc

#define SDL_STBIMAGE_IMPLEMENTATION
#include "SDL_stbimage.h"
#include <SDL.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

SDL_Surface* IMG_Load_RW(SDL_RWops *src, int freesrc) {
  assert(src->type == RW_TYPE_MEM);
  assert(freesrc == 0);
  return NULL;
}

SDL_Surface *IMG_Load(const char *filename) {
  int fd = open(filename, 0, 0);
  if (fd < 0) return NULL;

  int size = lseek(fd, 0, SEEK_END);
  if (size <= 0) {
    close(fd);
    return NULL;
  }

  void *buf = malloc(size);
  if (!buf) {
    close(fd);
    return NULL;
  }

  lseek(fd, 0, SEEK_SET);
  read(fd, buf, size);

  SDL_Surface *surface = STBIMG_LoadFromMemory((char *)buf, size);

  // 4. 清理资源
  free(buf);
  close(fd);

  return surface;
}

int IMG_isPNG(SDL_RWops *src) {
  return 0;
}

SDL_Surface* IMG_LoadJPG_RW(SDL_RWops *src) {
  return IMG_Load_RW(src, 0);
}

char *IMG_GetError() {
  return "Navy does not support IMG_GetError()";
}
