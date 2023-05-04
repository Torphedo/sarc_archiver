/*
 * High-level PhysicsFS archiver for simple Nintendo SARC archives. Based on
 * physfs_archiver_unpacked.c by Ryan C. Gordon. The original source file is
 * in the ext/physfs/src folder.
 *
 * RULES: Archive entries must be uncompressed. Dirs and files allowed, but no
 *  symlinks, etc. We can relax some of these rules as necessary.
 *
 * ZSTD compression can be handled using a custom PHYSFS_Io interface.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file originally written by Ryan C. Gordon.
 */

#include <stdint.h>
#include <stdlib.h>

#include <physfs.h>
#define __PHYSICSFS_INTERNAL__
#include <physfs_internal.h>

#include "sarc.h"
#include "archiver_sarc.h"
#include "custom_stream_standard.h"

const PHYSFS_Archiver archiver_sarc_default = {
        .version = 0,
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

void SARC_closeArchive(void *opaque) {
    SARCinfo *info = ((SARCinfo *) opaque);
    if (info)
    {
        __PHYSFS_DirTreeDeinit(&info->tree);

        if (info->io)
            info->io->destroy(info->io);

        allocator.Free(info);
    } /* if */
} /* SARC_closeArchive */

void SARC_abandonArchive(void *opaque) {
    SARCinfo *info = ((SARCinfo *) opaque);
    if (info)
    {
        info->io = NULL;
        SARC_closeArchive(info);
    } /* if */
} /* SARC_abandonArchive */

static inline SARCentry *findEntry(SARCinfo *info, const char *path) {
    return (SARCentry *) __PHYSFS_DirTreeFind(&info->tree, path);
} /* findEntry */

PHYSFS_Io *SARC_openRead(void *opaque, const char *name) {
    PHYSFS_Io *retval = NULL;
    SARCinfo *info = (SARCinfo *) opaque;
    SARCfileinfo *finfo = NULL;
    SARCentry *entry = findEntry(info, name);

    BAIL_IF_ERRPASS(!entry, NULL);
    BAIL_IF(entry->tree.isdir, PHYSFS_ERR_NOT_A_FILE, NULL);

    retval = (PHYSFS_Io *) allocator.Malloc(sizeof (PHYSFS_Io));
    GOTO_IF(!retval, PHYSFS_ERR_OUT_OF_MEMORY, SARC_openRead_failed);

    finfo = (SARCfileinfo *) allocator.Malloc(sizeof (SARCfileinfo));
    GOTO_IF(!finfo, PHYSFS_ERR_OUT_OF_MEMORY, SARC_openRead_failed);

    finfo->io = info->io->duplicate(info->io);
    GOTO_IF_ERRPASS(!finfo->io, SARC_openRead_failed);

    if (!finfo->io->seek(finfo->io, entry->startPos))
        goto SARC_openRead_failed;

    finfo->curPos = 0;
    finfo->entry = entry;

    // Set SARC_Io as the I/O handler for this archiver
    memcpy(retval, &SARC_Io, sizeof (*retval));
    retval->opaque = finfo;
    return retval;

SARC_openRead_failed:
    if (finfo != NULL)
    {
        if (finfo->io != NULL)
            finfo->io->destroy(finfo->io);
        allocator.Free(finfo);
    } /* if */

    if (retval != NULL)
        allocator.Free(retval);

    return NULL;
} /* SARC_openRead */

PHYSFS_Io *SARC_openWrite(void *opaque, const char *name) {
    BAIL(PHYSFS_ERR_READ_ONLY, NULL);
} /* SARC_openWrite */

PHYSFS_Io *SARC_openAppend(void *opaque, const char *name) {
    BAIL(PHYSFS_ERR_READ_ONLY, NULL);
} /* SARC_openAppend */

int SARC_remove(void *opaque, const char *name) {
    BAIL(PHYSFS_ERR_READ_ONLY, 0);
} /* SARC_remove */

int SARC_mkdir(void *opaque, const char *name) {
    BAIL(PHYSFS_ERR_READ_ONLY, 0);
} /* SARC_mkdir */

int SARC_stat(void *opaque, const char *path, PHYSFS_Stat *stat) {
    SARCinfo *info = (SARCinfo *) opaque;
    const SARCentry *entry = findEntry(info, path);

    BAIL_IF_ERRPASS(!entry, 0);

    if (entry->tree.isdir) {
        stat->filetype = PHYSFS_FILETYPE_DIRECTORY;
        stat->filesize = 0;
    } /* if */
    else {
        stat->filetype = PHYSFS_FILETYPE_REGULAR;
        stat->filesize = entry->size;
    } /* else */

    stat->modtime = entry->mtime;
    stat->createtime = entry->ctime;
    stat->accesstime = -1;
    stat->readonly = 1;

    return 1;
} /* SARC_stat */

void* SARC_addEntry(void* opaque, char* name, const int isdir,
                    const PHYSFS_sint64 ctime, const PHYSFS_sint64 mtime,
                    const PHYSFS_uint64 pos, const PHYSFS_uint64 len) {
    SARCinfo* info = (SARCinfo*) opaque;
    SARCentry* entry;

    entry = (SARCentry*) __PHYSFS_DirTreeAdd(&info->tree, name, isdir);
    BAIL_IF_ERRPASS(!entry, NULL);

    entry->startPos = isdir ? 0 : pos;
    entry->size = isdir ? 0 : len;
    entry->ctime = ctime;
    entry->mtime = mtime;

    return entry;
} /* SARC_addEntry */

void* SARC_init_archive(PHYSFS_Io* io) {
    SARCinfo* info = (SARCinfo *) allocator.Malloc(sizeof (SARCinfo));
    BAIL_IF(!info, PHYSFS_ERR_OUT_OF_MEMORY, NULL);

    bool case_sensitive = true;
    bool only_us_ascii = false;

    if (!__PHYSFS_DirTreeInit(&info->tree, sizeof (SARCentry), case_sensitive, only_us_ascii)) {
        allocator.Free(info);
        return NULL;
    }
    info->io = io;

    return info;
}

bool SARC_loadEntries(PHYSFS_Io* io, uint32_t count, uint32_t files_offset, SARCinfo* archive) {
    uint32_t name_pos = sizeof(sarc_header) + sizeof(sarc_sfat_header);
    name_pos += (sizeof(sarc_sfat_node) * count) + sizeof(sarc_sfnt_header);
    uint32_t name_buf_size = files_offset - name_pos;

    char* name_buffer = allocator.Malloc(name_buf_size);
    if (name_buffer == NULL) {
        PHYSFS_setErrorCode(PHYSFS_ERR_OUT_OF_MEMORY);
        return false;
    }

    // Save our place, jump to the list of names
    uint32_t read_pos = io->tell(io);
    io->seek(io, name_pos);
    io->read(io, name_buffer, name_buf_size);
    io->seek(io, read_pos);

    name_pos = 0; // Reset name position offset so we start reading from the first filename.

    for (uint32_t i = 0; i < count; i++) {
        sarc_sfat_node node = {0};
        io->read(io, &node, sizeof(node));
        uint32_t size = node.file_end_offset - node.file_start_offset;

        // Jump to the next 4-byte alignment boundary
        while((name_pos % 4) != 0) {
            name_pos++;
        }

        uint32_t file_pos = node.file_start_offset + files_offset;

        char* name = name_buffer + name_pos;
        SARC_addEntry(archive, name, 0, -1, -1, file_pos, size);
        name_pos += strlen(name) + 1;
    }
    allocator.Free(name_buffer);

    return true;
}

void* SARC_openArchive(PHYSFS_Io* io, const char* name, int forWriting, int* claimed) {
    assert(io != NULL); // Sanity check.
    
    static const char magic[] = "SARC";
    sarc_header header = {0};
    io->read(io, &header, sizeof(header));

    if (strncmp((char*) &header.magic, magic, 4) != 0) {
        BAIL(PHYSFS_ERR_UNSUPPORTED, NULL);
    }

    // Claim the archive, because it's a valid SARC
    *claimed = 1;

    sarc_sfat_header sfat_header = {0};
    io->read(io, &sfat_header, sizeof(sfat_header));

    SARCinfo* archive = SARC_init_archive(io);
    BAIL_IF_ERRPASS(!archive, NULL);

    SARC_loadEntries(io, sfat_header.node_count, header.data_offset, archive);

    return archive;
}
