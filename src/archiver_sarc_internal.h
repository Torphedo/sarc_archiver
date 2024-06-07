#pragma once
#include <physfs.h>
#define __PHYSICSFS_INTERNAL__
#include <physfs_internal.h>

#include <stdint.h>

typedef struct {
    __PHYSFS_DirTreeEntry tree;
    PHYSFS_uint64 startPos;
    PHYSFS_uint64 size;
    uintptr_t data_ptr; // Files open for write will store a pointer here instead of an offset.
}SARCentry;

// Archiver context for each SARC archive
typedef struct {
    __PHYSFS_DirTree tree;
    PHYSFS_Io *io;
    uint32_t open_write_handles; // The number of write handles currently open to this archive
    char* arc_filename;
}SARC_ctx;

// Context for each IO stream (file inside a SARC)
typedef struct {
    PHYSFS_Io *io;
    SARCentry *entry;
    SARC_ctx* arc_info;
    PHYSFS_uint32 curPos;
}SARC_file_ctx;

SARCentry *findEntry(SARC_ctx* ctx, const char *path);

