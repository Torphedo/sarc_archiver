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

static const char* module_name_info = "[\033[32mVFS::SARC\033[0m]";
static const char* module_name_warn = "[\033[33mVFS::SARC\033[0m]";
static const char* module_name_err = "[\033[31mVFS::SARC\033[0m]";

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

typedef struct {
    __PHYSFS_DirTree tree;
    PHYSFS_Io *io;
}SARCinfo;

typedef struct {
    __PHYSFS_DirTreeEntry tree;
    PHYSFS_uint64 startPos;
    uintptr_t data_ptr; // Files open for write will store a pointer here instead of an offset.
    PHYSFS_uint64 size;
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

// Copy all file contents to newly allocated buffers
PHYSFS_EnumerateCallbackResult callback(void *data, const char *origdir, const char *fname) {
    SARCinfo* info = (SARCinfo*)data;
    char* full_path = __PHYSFS_smallAlloc(strlen(origdir) + strlen(fname) + 1);
    // It doesn't want a leading slash, but we need a slash between directories.
    if (origdir[0] == 0) {
        // No containing dir.
        strcpy(full_path, fname);
    }
    else {
        sprintf(full_path, "%s/%s", origdir, fname);
    }

    PHYSFS_Stat statbuf = {0};
    PHYSFS_stat(full_path, &statbuf);
    if  (statbuf.filetype == PHYSFS_FILETYPE_DIRECTORY){
        __PHYSFS_DirTreeEnumerate(&info->tree, full_path, callback, full_path, data);
    }
    else {
        // We've finally got a full filename.
        SARCentry* entry = findEntry(info, full_path);

        // Store the file in a new buffer and store the pointer in the entry.
        entry->data_ptr = (uint64_t) allocator.Malloc(entry->size);
        uint64_t pos = info->io->tell(info->io); // Save position
        info->io->seek(info->io, entry->startPos);
        info->io->read(info->io, (void*)entry->data_ptr, entry->size);
        info->io->seek(info->io, pos); // Go back to saved position
        printf("%s %s\n", module_name_info, full_path);
    }


    __PHYSFS_smallFree(full_path);
    return PHYSFS_ENUM_OK;
}

PHYSFS_Io* SARC_openWrite(void *opaque, const char *name) {
    // Work in progress.
    SARCinfo* info = (SARCinfo*) opaque;
    printf("%s SARC_openWrite() called.\n", module_name_info);
    __PHYSFS_DirTree* tree = (__PHYSFS_DirTree *) &info->tree;

    __PHYSFS_DirTreeEnumerate(tree, "", callback, "", opaque);
    return NULL;
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

    stat->modtime = 0;
    stat->createtime = 0;
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


// PHYSFS_Io implementation for SARC

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
