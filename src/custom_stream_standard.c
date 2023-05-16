#define __PHYSICSFS_INTERNAL__
#include <physfs_internal.h>

#include "custom_stream_standard.h"

typedef struct {
    __PHYSFS_DirTree tree;
    PHYSFS_Io *io;
}SARCinfo;

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

PHYSFS_sint64 SARC_read(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 len) {
    SARCfileinfo *finfo = (SARCfileinfo *) io->opaque;
    const SARCentry *entry = finfo->entry;
    const PHYSFS_uint64 bytesLeft = (PHYSFS_uint64)(entry->size - finfo->curPos);
    PHYSFS_sint64 rc;

    if (bytesLeft < len)
        len = bytesLeft;

    rc = finfo->io->read(finfo->io, buffer, len);
    if (rc > 0)
        finfo->curPos += (PHYSFS_uint32) rc;

    return rc;
} /* SARC_read */

PHYSFS_sint64 SARC_write(PHYSFS_Io *io, const void *b, PHYSFS_uint64 len) {
    BAIL(PHYSFS_ERR_READ_ONLY, -1);
} /* SARC_write */

PHYSFS_sint64 SARC_tell(PHYSFS_Io *io) {
    return ((SARCfileinfo *) io->opaque)->curPos;
} /* SARC_tell */

int SARC_seek(PHYSFS_Io *io, PHYSFS_uint64 offset) {
    SARCfileinfo *finfo = (SARCfileinfo *) io->opaque;
    const SARCentry *entry = finfo->entry;
    int rc;

    BAIL_IF(offset >= entry->size, PHYSFS_ERR_PAST_EOF, 0);
    rc = finfo->io->seek(finfo->io, entry->startPos + offset);
    if (rc)
        finfo->curPos = (PHYSFS_uint32) offset;

    return rc;
} /* SARC_seek */

PHYSFS_sint64 SARC_length(PHYSFS_Io *io) {
    const SARCfileinfo *finfo = (SARCfileinfo *) io->opaque;
    return ((PHYSFS_sint64) finfo->entry->size);
} /* SARC_length */

PHYSFS_Io *SARC_duplicate(PHYSFS_Io *_io) {
    SARCfileinfo *origfinfo = (SARCfileinfo *) _io->opaque;
    PHYSFS_Io *io = NULL;
    PHYSFS_Io *retval = (PHYSFS_Io *) allocator.Malloc(sizeof (PHYSFS_Io));
    SARCfileinfo *finfo = (SARCfileinfo *) allocator.Malloc(sizeof (SARCfileinfo));
    GOTO_IF(!retval, PHYSFS_ERR_OUT_OF_MEMORY, SARC_duplicate_failed);
    GOTO_IF(!finfo, PHYSFS_ERR_OUT_OF_MEMORY, SARC_duplicate_failed);

    io = origfinfo->io->duplicate(origfinfo->io);
    if (!io) goto SARC_duplicate_failed;
    finfo->io = io;
    finfo->entry = origfinfo->entry;
    finfo->curPos = 0;
    memcpy(retval, _io, sizeof (PHYSFS_Io));
    retval->opaque = finfo;
    return retval;

    SARC_duplicate_failed:
    if (finfo != NULL) allocator.Free(finfo);
    if (retval != NULL) allocator.Free(retval);
    if (io != NULL) io->destroy(io);
    return NULL;
} /* SARC_duplicate */

int SARC_flush(PHYSFS_Io *io) { return 1;  /* no write support. */ }

void SARC_destroy(PHYSFS_Io *io) {
    SARCfileinfo *finfo = (SARCfileinfo *) io->opaque;
    finfo->io->destroy(finfo->io);
    allocator.Free(finfo);
    allocator.Free(io);
} /* SARC_destroy */
