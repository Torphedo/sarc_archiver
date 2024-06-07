#pragma once
#include <stdbool.h>

#include <physfs.h>

// This is the main SARC archiver implementation, exposing the SARC's contents
// to PHYSFS.

void SARC_closeArchive(void *opaque);

void SARC_abandonArchive(void *opaque);

PHYSFS_Io* SARC_openRead(void* opaque, const char* name);
PHYSFS_Io* SARC_openWrite(void* opaque, const char* name);
PHYSFS_Io* SARC_openAppend(void* opaque, const char* name);
int SARC_remove(void* opaque, const char* name);
int SARC_mkdir(void* opaque, const char* name);
int SARC_stat(void* opaque, const char* path, PHYSFS_Stat* stat);
void* SARC_openArchive(PHYSFS_Io* io, const char* name, int forWriting, int* claimed);

// Archiver structs to register
#ifdef __cplusplus
extern "C" {
#endif
extern const PHYSFS_Archiver archiver_sarc_default;
#ifdef __cplusplus
}
#endif
