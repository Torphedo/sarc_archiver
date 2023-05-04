#pragma once
// Custom PHYSFS_Io file stream that wraps around the normal PhysicsFS I/O
#include <stdlib.h>

#include <physfs.h>

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
        .version = 0,
        .opaque = NULL,
        .read = SARC_read,
        .write = SARC_write,
        .seek = SARC_seek,
        .tell = SARC_tell,
        .length = SARC_length,
        .duplicate = SARC_duplicate,
        .flush = SARC_flush,
        .destroy = SARC_destroy
};
