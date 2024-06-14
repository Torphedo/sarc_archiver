#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <int.h>
#include <physfs.h>
// This is a PHYSFS_Io (file I/O interface) implementation for zstd-compressed
// files.

// Wrap an existing IO stream with ZSTD, to transparently handle (de)compression
PHYSFS_Io* zstd_wrap_io(PHYSFS_Io* io);
void zstd_io_add_dict(const char* path);

// Custom IO
PHYSFS_sint64 zstd_read(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 len);
PHYSFS_sint64 zstd_write(PHYSFS_Io *io, const void *b, PHYSFS_uint64 len);
int zstd_seek(PHYSFS_Io* io, PHYSFS_uint64 offset);
PHYSFS_sint64 zstd_tell(PHYSFS_Io *io);
PHYSFS_sint64 zstd_length(PHYSFS_Io* io);
PHYSFS_Io *zstd_duplicate(PHYSFS_Io* _io);
int zstd_flush(PHYSFS_Io* io);
void zstd_destroy(PHYSFS_Io* io);

static const PHYSFS_Io ZSTD_Io = {
    .read = zstd_read,
    .write = zstd_write,
    .seek = zstd_seek,
    .tell = zstd_tell,
    .length = zstd_length,
    .duplicate = zstd_duplicate,
    .flush = zstd_flush,
    .destroy = zstd_destroy,
};

#ifdef __cplusplus
}
#endif