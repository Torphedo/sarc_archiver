#pragma once
#include <physfs.h>
// This is a PHYSFS_Io (file I/O interface) implementation for the SARC
// archiver. It uses seeks/reads & internal buffers to emulate normal streaming
// behaviour.

// Custom IO
PHYSFS_sint64 SARC_read(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 len);
PHYSFS_sint64 SARC_write(PHYSFS_Io *io, const void *b, PHYSFS_uint64 len);
PHYSFS_sint64 SARC_tell(PHYSFS_Io *io);
int SARC_seek(PHYSFS_Io* io, PHYSFS_uint64 offset);
PHYSFS_sint64 SARC_length(PHYSFS_Io* io);
int SARC_trunc(PHYSFS_Io* io, PHYSFS_uint64 len);
PHYSFS_Io *SARC_duplicate(PHYSFS_Io* _io);
int SARC_flush(PHYSFS_Io* io);
void SARC_destroy(PHYSFS_Io* io);

static const PHYSFS_Io SARC_Io = {
    .version = 0,
    .opaque = NULL,
    .read = SARC_read,
    .write = SARC_write,
    .seek = SARC_seek,
    .tell = SARC_tell,
    .length = SARC_length,
    .trunc = SARC_trunc,
    .duplicate = SARC_duplicate,
    .flush = SARC_flush,
    .destroy = SARC_destroy
};
