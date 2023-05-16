#pragma once
#include <stdbool.h>

void SARC_closeArchive(void *opaque);

void SARC_abandonArchive(void *opaque);

// Custom IO
PHYSFS_sint64 SARC_read(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 len);
PHYSFS_sint64 SARC_write(PHYSFS_Io *io, const void *b, PHYSFS_uint64 len);
PHYSFS_sint64 SARC_tell(PHYSFS_Io *io);
int SARC_seek(PHYSFS_Io* io, PHYSFS_uint64 offset);
PHYSFS_sint64 SARC_length(PHYSFS_Io* io);
PHYSFS_Io *SARC_duplicate(PHYSFS_Io* _io);
int SARC_flush(PHYSFS_Io* io);
void SARC_destroy(PHYSFS_Io* io);

PHYSFS_Io* SARC_openRead(void* opaque, const char* name);
PHYSFS_Io* SARC_openWrite(void* opaque, const char* name);
PHYSFS_Io* SARC_openAppend(void* opaque, const char* name);
int SARC_remove(void* opaque, const char* name);
int SARC_mkdir(void* opaque, const char* name);
int SARC_stat(void* opaque, const char* path, PHYSFS_Stat* stat);
void* SARC_addEntry(void* opaque, char* name, const int isdir, const PHYSFS_sint64 ctime, const PHYSFS_sint64 mtime, const PHYSFS_uint64 pos, const PHYSFS_uint64 len);
void* SARC_openArchive(PHYSFS_Io* io, const char* name, int forWriting, int* claimed);

// Archiver structs to register
const extern PHYSFS_Archiver archiver_sarc_default;
