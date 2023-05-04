#pragma once

#include <stdbool.h>

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

typedef struct {
    __PHYSFS_DirTreeEntry tree;
    PHYSFS_uint64 startPos;
    PHYSFS_uint64 size;
    PHYSFS_sint64 ctime;
    PHYSFS_sint64 mtime;
}SARCentry;

typedef struct {
    PHYSFS_Io *io;
    SARCentry *entry;
    PHYSFS_uint32 curPos;
}SARCfileinfo;

void SARC_closeArchive(void *opaque);

void SARC_abandonArchive(void *opaque);

// Custom IO
PHYSFS_sint64 SARC_read(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 len);
PHYSFS_sint64 SARC_write(PHYSFS_Io *io, const void *b, PHYSFS_uint64 len);
PHYSFS_sint64 SARC_tell(PHYSFS_Io *io);
int SARC_seek(PHYSFS_Io *io, PHYSFS_uint64 offset);
PHYSFS_sint64 SARC_length(PHYSFS_Io *io);
PHYSFS_Io *SARC_duplicate(PHYSFS_Io *_io);
int SARC_flush(PHYSFS_Io *io);
void SARC_destroy(PHYSFS_Io *io);

static const PHYSFS_Io SARC_Io = {
    CURRENT_PHYSFS_IO_API_VERSION, NULL,
    SARC_read,
    SARC_write,
    SARC_seek,
    SARC_tell,
    SARC_length,
    SARC_duplicate,
    SARC_flush,
    SARC_destroy
};

PHYSFS_Io *SARC_openRead(void *opaque, const char *name);
PHYSFS_Io *SARC_openWrite(void *opaque, const char *name);
PHYSFS_Io *SARC_openAppend(void *opaque, const char *name);
int SARC_remove(void *opaque, const char *name);
int SARC_mkdir(void *opaque, const char *name);
int SARC_stat(void *opaque, const char *path, PHYSFS_Stat *stat);
void *SARC_addEntry(void *opaque, char *name, const int isdir, const PHYSFS_sint64 ctime, const PHYSFS_sint64 mtime, const PHYSFS_uint64 pos, const PHYSFS_uint64 len);
void* SARC_openArchive(PHYSFS_Io* io, const char* name, int forWriting, int* claimed);

// Archiver structs to register
static const PHYSFS_Archiver archiver_sarc_default = {
    .version = CURRENT_PHYSFS_ARCHIVER_API_VERSION,
    .info = {
        .extension = "pack",
        .description = "An extension to support uncompressed SARC files with a .pack extension",
        .author = "Torphedo",
        .url = "https://github.com/Torphedo",
        .supportsSymlinks = false
    },
    .openArchive = SARC_openArchive,
    .enumerate = __PHYSFS_DirTreeEnumerate,
    .openRead = SARC_openRead,
    .openWrite = SARC_openWrite,
    .openAppend = SARC_openAppend,
    .remove = SARC_remove,
    .mkdir = SARC_mkdir,
    .stat = SARC_stat,
    .closeArchive = SARC_closeArchive
};

