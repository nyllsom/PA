#include <proc.h>
#include <fs.h>
#include <elf.h>
#include <common.h>
#include <memory.h>

#ifdef __LP64__
# define Elf_Ehdr Elf64_Ehdr
# define Elf_Phdr Elf64_Phdr
#else
# define Elf_Ehdr Elf32_Ehdr
# define Elf_Phdr Elf32_Phdr
#endif

#if defined(__ISA_AM_NATIVE__)
# define EXPECT_TYPE EM_X86_64
#elif defined(__ISA_X86__)
# define EXPECT_TYPE EM_386
#elif defined(__ISA_RISCV32__)
# define EXPECT_TYPE EM_RISCV
#elif defined(__ISA_MIPS32__)
# define EXPECT_TYPE EM_MIPS
#else
# error Unsupported ISA
#endif

extern size_t ramdisk_read(void *buf, size_t offset, size_t len);
extern int fs_open(const char *pathname, int flags, int mode);
extern size_t fs_read(int fd, void *buf, size_t len);
extern size_t fs_write(int fd, const void *buf, size_t len);
extern size_t fs_lseek(int fd, size_t offset, int whence);
extern int fs_close(int fd);

static uintptr_t loader(PCB *pcb, const char *filename) {
  Elf_Ehdr ehdr;
  int fd = fs_open(filename, 0, 0);   // TODO: use flags and mode
  assert(fd >= 0);

  assert(fs_read(fd, &ehdr, sizeof(Elf_Ehdr)) == sizeof(Elf_Ehdr));

  assert(*(uint32_t *)ehdr.e_ident == 0x464c457f);
  assert(ehdr.e_machine == EXPECT_TYPE);
  assert(ehdr.e_phentsize == sizeof(Elf_Phdr));

  for (int i = 0; i < ehdr.e_phnum; i++) {
    Elf_Phdr phdr;

    size_t phoff = ehdr.e_phoff + (size_t)i * sizeof(Elf_Phdr);
    fs_lseek(fd, phoff, SEEK_SET);
    assert(fs_read(fd, &phdr, sizeof(Elf_Phdr)) == sizeof(Elf_Phdr));

    if (phdr.p_type != PT_LOAD) continue;

    assert(phdr.p_memsz >= phdr.p_filesz);

    uintptr_t vaddr = (uintptr_t)phdr.p_vaddr;

    if (phdr.p_filesz > 0) {
      fs_lseek(fd, phdr.p_offset, SEEK_SET);
      assert(fs_read(fd, (void *)vaddr, phdr.p_filesz) == phdr.p_filesz);
    }

    if (phdr.p_memsz > phdr.p_filesz) {
      memset((void *)(vaddr + phdr.p_filesz), 0, phdr.p_memsz - phdr.p_filesz);
    }
  }

  fs_close(fd);

  // 4. 返回入口地址
  return (uintptr_t)ehdr.e_entry;
}

void naive_uload(PCB *pcb, const char *filename) {
  uintptr_t entry = loader(pcb, filename);
  Log("Jump to entry = %p", (void *)entry);
  ((void(*)())entry) (); // transfer entry into a function pointer
}
