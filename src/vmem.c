#include <stdlib.h> // For NULL
#include <stdint.h> // For uint64_t

// Platform definition macros.

//  MacOS Classic,      or Mac OS X.
#if defined(macintosh) || (defined(__APPLE__) && defined(__MACH__))
#define PLATFORM_MACOS
#endif

// All the various non-Apple Unix-based platforms.
#if defined(linux) || defined(__linux__) || defined(__UNIX__) || defined(__unix__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define PLATFORM_UNIX
#endif

#ifdef _WIN32
#define PLATFORM_WINDOWS
#endif

// Unix / POSIX implementation
#if defined(PLATFORM_UNIX) || defined(PLATFORM_MACOS)
#include <sys/mman.h>

void* virtual_reserve(uint64_t size) {
  return mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}
int virtual_commit(void* addr, uint64_t size) {
  return mprotect(addr, size, PROT_READ | PROT_WRITE);
}
int virtual_free(void* addr, uint64_t size) {
  return munmap(addr, size);
}
#endif

#ifdef PLATFORM_WINDOWS
#include <Windows.h>

void* virtual_reserve(uint64_t size) {
  return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_READWRITE);
}
int virtual_commit(void* addr, uint64_t size) {
  return VirtualAlloc(addr, size, MEM_COMMIT, PAGE_READWRITE);
}
int virtual_free(void* addr, uint64_t size) {
  return VirtualFree(addr, 0, MEM_RELEASE);
}
#endif

// Reserving / committing doesn't really exist as a kernel concept on HorizonOS
// (Switch). We'll probably have to fake this behaviour by doing re-allocation
// and copying behind the scenes to get it to work.

