#include <fs.h>

typedef size_t (*ReadFn) (void *buf, size_t offset, size_t len);
typedef size_t (*WriteFn) (const void *buf, size_t offset, size_t len);

size_t ramdisk_read(void *buf, size_t offset, size_t len);
size_t ramdisk_write(const void *buf, size_t offset, size_t len);

typedef struct {
  char *name;
  size_t size;
  size_t disk_offset;
  ReadFn read;
  WriteFn write;
  size_t open_offset;
} Finfo;

enum {FD_STDIN, FD_STDOUT, FD_STDERR, FD_FB};

size_t invalid_read(void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

size_t invalid_write(const void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

extern size_t serial_write(const void *buf, size_t offset, size_t len);
extern size_t events_read(void *buf, size_t offset, size_t len);
extern size_t fb_write(const void *buf, size_t offset, size_t len);
extern size_t dispinfo_read(void *buf, size_t offset, size_t len);


static Finfo file_table[] = {
  [FD_STDIN]  = {"stdin",  0, 0, invalid_read,  invalid_write},
  [FD_STDOUT] = {"stdout", 0, 0, invalid_read,  serial_write},
  [FD_STDERR] = {"stderr", 0, 0, invalid_read,  serial_write},
  [FD_FB]     = {"/dev/fb", 0, 0, invalid_read, fb_write},
#include "files.h"
  {"/dev/events", 0, 0, events_read, invalid_write},
  {"/proc/dispinfo", 0, 0, dispinfo_read, invalid_write},
};

// void init_fs() {
//   AM_GPU_CONFIG_T cfg = io_read(AM_GPU_CONFIG);
//   file_table[FD_FB].size = cfg.vmemsz; 
// }

void init_fs() {
  AM_GPU_CONFIG_T cfg = io_read(AM_GPU_CONFIG);
  // printf("[init_fs] GPU cfg: w=%d h=%d vmemsz=%d\n",
  //        cfg.width, cfg.height, cfg.vmemsz);
  file_table[FD_FB].size = (size_t)cfg.width * cfg.height * 4;
}

static const int nr_files = sizeof(file_table) / sizeof(file_table[0]);

/* =====================  fs_open  ===================== */

int fs_open(const char *pathname, int flags, int mode) {
  for (int i = 0; i < nr_files; i++) {
    if (strcmp(pathname, file_table[i].name) == 0) {
      file_table[i].open_offset = 0;
      // Log("fs_open: open '%s'", pathname);
      return i;
    }
  }
  Log("fs_open: no such file '%s'", pathname);
  return -1;
}

/* =====================  fs_read  ===================== */

size_t fs_read(int fd, void *buf, size_t len) {
  Finfo *f = &file_table[fd];

  // 设备文件和普通文件读写逻辑不同，设备文件不需要检查 size 和 offset，因为均设置为 0，代表它不是真的在 disk 上面的东西 
  if (f->read != NULL && f->read != invalid_read) {
    size_t ret = f->read(buf, f->open_offset, len);
    f->open_offset += ret;
    return ret;
  }

  if (f->open_offset >= f->size) {
    return 0;
  }

  size_t max_len = f->size - f->open_offset;
  if (len > max_len) len = max_len;

  size_t ret = ramdisk_read(buf, f->disk_offset + f->open_offset, len);
  f->open_offset += ret;
  return ret;
}


/* =====================  fs_write  ===================== */

size_t fs_write(int fd, const void *buf, size_t len) {
  Finfo *f = &file_table[fd];

  if (f->write != NULL && f->write != invalid_write) {
    size_t ret = f->write(buf, f->open_offset, len);
    f->open_offset += ret;
    return ret;
  }

  if (f->open_offset >= f->size) {
    return 0; // OR: 截断在末尾
  }

  size_t max_len = f->size - f->open_offset;
  if (len > max_len) len = max_len;

  size_t ret = ramdisk_write(buf, f->disk_offset + f->open_offset, len);
  f->open_offset += ret;
  return ret;
}

/* =====================  fs_lseek  ===================== */

size_t fs_lseek(int fd, size_t offset, int whence) {
  Finfo *f = &file_table[fd];
  size_t new_offset = 0;

  switch (whence) {
    case SEEK_SET: new_offset = offset; break;
    case SEEK_CUR: new_offset = f->open_offset + offset; break;
    case SEEK_END: new_offset = f->size + offset; break;
    default: 
      panic("Invalid whence %d", whence);
  }

  if (new_offset > f->size) new_offset = f->size;
  f->open_offset = new_offset;
  return new_offset;
}

/* =====================  fs_close  ===================== */

int fs_close(int fd) {
  return 0;
}