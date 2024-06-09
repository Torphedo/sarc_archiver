#include <physfs.h>
#define __PHYSICSFS_INTERNAL__
#include <physfs_internal.h>

#include "sarc.h"
#include "archiver_sarc_internal.h"
#include "sarc_io.h"
#include "vmem.h"
#include "physfs_utils.h"
#include "logging.h"

uint32_t get_file_list_count(__PHYSFS_DirTreeEntry* entry) {
    uint32_t retval = 0;

    while (entry != NULL) {
        if (!entry->isdir)
            retval++;
        else
            retval += get_file_list_count(entry->children);
        entry = entry->sibling;
    }

    return retval;
}

void write_file_list(__PHYSFS_DirTreeEntry* entry, char** arr, int* currentIndex) {
    while (entry != NULL) {
        if (!entry->isdir) {
            arr[*currentIndex] = entry->name;
            (*currentIndex)++;
        }
        else
            write_file_list(entry->children, arr, currentIndex);
        entry = entry->sibling;
    }
}

char** get_file_list(__PHYSFS_DirTreeEntry* entry) {
    uint32_t count = get_file_list_count(entry);
    char** retval = allocator.Malloc(sizeof(char*) * (count + 1));
    memset(retval, 0x0, sizeof(char*) * (count + 1));
    int currentIndex = 0;
    write_file_list(entry, retval, &currentIndex);
    retval[count] = NULL;
    return retval;
}

// Update the SARC file on disk that this IO stream (file) belongs to.
void rebuild_sarc(SARC_ctx* ctx) {
    PHYSFS_Io* io = ctx->io;

    sarc_header header = {
        .magic = SARC_MAGIC,
        .header_size = SARC_HEADER_SIZE,
        .byte_order_mark = SARC_LITTLE_ENDIAN,
        .archive_size = 0, // We fill these 2 fields in at the end.
        .data_offset = 0,
        .version = SARC_VERSION,
        .reserved = 0
    };
    sarc_sfat_header sfat_header = {
        .magic = SFAT_MAGIC,
        .header_size = SFAT_HEADER_SIZE,
        .node_count = 0, // Number of files in archive
        .hash_key = SFAT_HASH_KEY
    };
    sarc_sfnt_header sfnt_header = {
        .magic = SFNT_MAGIC,
        .header_size = SFNT_HEADER_SIZE,
        .reserved = 0
    };
    char* name = ctx->arc_filename;

    // We'll write these later, skip for now.
    io->seek(io, sizeof(header) + sizeof(sfat_header));

    __PHYSFS_DirTree* tree = &ctx->tree;
    char** file_list_unsorted = get_file_list(tree->root);

    // Get file count first.
    for (char** i = file_list_unsorted; *i != NULL; i++) {
        sfat_header.node_count++;
    }

    // The files are ordered by hash, so we need to sort them before writing.
    uint32_t sorted = 0;

    uint32_t size = sizeof(char*) * (sfat_header.node_count + 1);
    char** file_list = allocator.Malloc(size);
    memset(file_list, 0x00, size);

    // This is a really terrible sorting algorithm I wrote on the spot.
    // TODO: See if __PHYSFS_sort() can be used here?
    // TODO: Replace this with quicksort or something. The reduced syscalls alone
    //       should make it worthwhile.
    while(sorted < sfat_header.node_count) {
        uint32_t smallest_hash = 0;
        for (char** i = file_list_unsorted; *i != NULL; i++) {
            if (*i == (void*)1) {
                continue;
            }
            // Hash and store the last string in the list that hasn't been eliminated yet
            smallest_hash = sarc_filename_hash(*i, strlen(*i), sfat_header.hash_key);
        }
        for (char** i = file_list_unsorted; *i != NULL; i++) {
            // Skip entries that have already been sorted
            if (*i == (void*)1) {
                continue;
            }
            uint32_t hash = sarc_filename_hash(*i, strlen(*i), sfat_header.hash_key);
            if (hash < smallest_hash) {
                // By the end of the loop, smallest_hash will have the smallest hash.
                smallest_hash = hash;
            }
        }
        for (char** i = file_list_unsorted; *i != NULL; i++) {
            if (*i == (void*)1) {
                continue;
            }
            uint32_t hash = sarc_filename_hash(*i, strlen(*i), sfat_header.hash_key);
            // Search for the smallest hash in the list, skipping sorted entries.
            if (hash == smallest_hash) {
                // Put the smallest hash next in the sorted list and free the original.
                file_list[sorted++] = *i;
                *i = (void*)1;
                break;
            }
        }
    }
    allocator.Free(file_list_unsorted);


    // Skip over SFAT until we know what offsets things should be at
    io->seek(io, io->tell(io) + (sfat_header.node_count * sizeof(sarc_sfat_node)));

    // Write SFNT
    io->write(io, &sfnt_header, sizeof(sfnt_header));
    uint32_t filename_pos = io->tell(io);
    const uint32_t filename_start = filename_pos;

    for (char** i = file_list; *i != NULL; i++) {
        io->seek(io, io->tell(io) + strlen(*i) + 1);
        while((io->tell(io) % 4) != 0) {
            io->seek(io, io->tell(io) + 1);
        }
    }

    header.data_offset = io->tell(io); // Now we know where files should start.
    // This tracks where we are while writing files.
    uint32_t file_write_pos = header.data_offset;

    // Time to write SFAT and SFNT filenames.
    io->seek(io, sizeof(header)); // Jump to SFAT header location.
    io->write(io, &sfat_header, sizeof(sfat_header));
    for (char** i = file_list; *i != NULL; i++) {
        SARCentry* entry = findEntry(ctx, *i);

        sarc_sfat_node node = {
            .filename_hash = sarc_filename_hash(*i, strlen(*i), sfat_header.hash_key),
            .enable_offset = 0x0100,
            .filename_offset = (filename_pos - filename_start) / 4,
            .file_start_offset = file_write_pos - header.data_offset,
            .file_end_offset = file_write_pos + entry->size - header.data_offset
        };
        uint32_t cur_pos = io->tell(io); // Save our spot
        // Write the file data.
        io->seek(io, file_write_pos);
        if ((void*)entry->data_ptr == NULL) {
            LOG_MSG(error, "invalid file data pointer!\n");
            return;
        }
        io->write(io, (void*)entry->data_ptr, entry->size);
        // Update our file write position and align to 4 byte boundary
        file_write_pos = io->tell(io);
        while((file_write_pos % 8) != 0) {
            file_write_pos++;
        }

        io->seek(io, filename_pos);
        io->write(io, *i, strlen(*i) + 1); // Write filenames
        // Jump to the next 4-byte alignment boundary.
        while(((io->tell(io)) % 4) != 0) {
            char null = 0x0;
            io->write(io, &null, 1); // Jump forward 1 byte
            //io->seek(io, io->tell(io) + 1); 
        }
        filename_pos = io->tell(io);

        // Jump back to where we were, and write the SFAT node.
        io->seek(io, cur_pos);
        io->write(io, &node, sizeof(node));
    }
    header.archive_size = file_write_pos;
    io->seek(io, 0);
    io->write(io, &header, sizeof(header));

    io->trunc(io, file_write_pos);

    allocator.Free(file_list);
}

// Rebuild archive and write to disk.
void close_write_handle(PHYSFS_Io* io) {
    SARC_file_ctx* ctx = io->opaque;
    ctx->arc_info->open_write_handles--;
    //if (ctx->arc_info->open_write_handles == 0)
    rebuild_sarc(ctx->arc_info);
}

// Resize a file's reserved memory region.
void resize_entry(SARCentry* entry, PHYSFS_uint64 len) {
    uint64_t commitSize;
    if (len <= entry->reserved)
        commitSize = len;
    else
        commitSize = len + 500;

    if (virtual_commit((void*)entry->data_ptr, commitSize) == -1) {
        void* newMemory = virtual_reserve(commitSize);
        virtual_free((void*)entry->data_ptr, entry->reserved);
        entry->data_ptr = (uintptr_t)newMemory;
    }
    entry->size = len;
    entry->reserved = commitSize;
}

// PHYSFS_Io implementation for SARC

PHYSFS_sint64 SARC_read(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 len) {
    SARC_file_ctx* file = (SARC_file_ctx*)io->opaque;
    const SARCentry *entry = file->entry;
    const PHYSFS_uint64 bytesLeft = (PHYSFS_uint64)(entry->size - file->curPos);
    PHYSFS_sint64 rc;

    if (bytesLeft < len) {
        len = bytesLeft;
    }

    rc = file->io->read(file->io, buffer, len);
    if (rc > 0) {
        file->curPos += (PHYSFS_uint32) rc;
    }

    return rc;
} /* SARC_read */

PHYSFS_sint64 SARC_write(PHYSFS_Io *io, const void* buf, PHYSFS_uint64 len) {
    SARC_file_ctx* file = (SARC_file_ctx*)io->opaque;
    SARCentry* entry = file->entry;

    // Sanity checks.
    if (buf == NULL) {
        LOG_MSG(error, "Trying to write %lli bytes from a nullptr!\n", len);
        return -1;
    }
    // Most writes are under 4MiB... warn for unusally large individual writes,
    // in case someone passed in a bad value.
    if (len > 0x400000) {
        // Sorry for the extremely long line.
        LOG_MSG(warning, "Writing %lli bytes from a buffer at 0x%p. Writing will proceed normally, this is just a friendly alert that you might've passed a bad value.\n", len, buf);
    }

    if ((void*)entry->data_ptr == NULL) {
        // Not to jinx myself, but this should NEVER happen because opening a write
        // handle automatically sets this up.
        LOG_MSG(error, "Tried to write to a file that isn't set up for writing.\n");
        BAIL(PHYSFS_ERR_READ_ONLY, -1);
    }

    // Since files open for writing are only in memory until they're flushed by
    // closing the handle, we just do a memcpy.
    if (file->curPos + len >= entry->size) {
        // We're out of space, time to expand. Expand enough to fit this entire
        // write plus 500 bytes.

        resize_entry(entry, entry->size + len);
    }

    memcpy((void*)((char*)entry->data_ptr + ((SARC_file_ctx*)io->opaque)->curPos), buf, len);
    ((SARC_file_ctx*)io->opaque)->curPos += len;

    return 0;
} /* SARC_write */

PHYSFS_sint64 SARC_tell(PHYSFS_Io *io) {
    return ((SARC_file_ctx*)io->opaque)->curPos;
} /* SARC_tell */

int SARC_seek(PHYSFS_Io *io, PHYSFS_uint64 offset) {
    SARC_file_ctx* file = (SARC_file_ctx*)io->opaque;
    const SARCentry *entry = file->entry;
    int rc = 0;

    BAIL_IF(offset > entry->size, PHYSFS_ERR_PAST_EOF, 0);
    if (!file->open_for_write)
        rc = file->io->seek(file->io, entry->startPos + offset);
    if (rc || file->open_for_write) {
        file->curPos = (PHYSFS_uint32) offset;
    }

    return rc;
} /* SARC_seek */

PHYSFS_sint64 SARC_length(PHYSFS_Io *io) {
    const SARC_file_ctx* file = (SARC_file_ctx*)io->opaque;
    return ((PHYSFS_sint64) file->entry->size);
} /* SARC_length */

int SARC_trunc(PHYSFS_Io* io, PHYSFS_uint64 len) {
    const SARC_file_ctx* file = (SARC_file_ctx*)io->opaque;
    (PHYSFS_sint64)file->entry->size = len;
    return 1;
} /* SARC_length */

PHYSFS_Io *SARC_duplicate(PHYSFS_Io *_io) {
    SARC_file_ctx *original_file = (SARC_file_ctx*)_io->opaque;
    PHYSFS_Io *io = NULL;
    PHYSFS_Io *retval = (PHYSFS_Io *) allocator.Malloc(sizeof (PHYSFS_Io));
    SARC_file_ctx *newfile = (SARC_file_ctx *) allocator.Malloc(sizeof (SARC_file_ctx));
    GOTO_IF(!retval, PHYSFS_ERR_OUT_OF_MEMORY, SARC_duplicate_failed);
    GOTO_IF(!newfile, PHYSFS_ERR_OUT_OF_MEMORY, SARC_duplicate_failed);

    io = original_file->io->duplicate(original_file->io);
    if (!io) goto SARC_duplicate_failed;
    newfile->io = io;
    newfile->entry = original_file->entry;
    newfile->curPos = 0;
    memcpy(retval, _io, sizeof (PHYSFS_Io));
    retval->opaque = newfile;
    return retval;

    SARC_duplicate_failed:
    if (newfile != NULL) allocator.Free(newfile);
    if (retval != NULL) allocator.Free(retval);
    if (io != NULL) io->destroy(io);
    return NULL;
} /* SARC_duplicate */

// TODO: Make this decrement write handle counter and rebuild SARC then flush to
// disk. If there are no more open write handles, free the individual file
// buffers and make it a normal read-only archive again. This function is only
// called when closing a write handle or shutting down PhysicsFS.
int SARC_flush(PHYSFS_Io *io) {
    close_write_handle(io);
    return 1;
}

void SARC_destroy(PHYSFS_Io *io) {
    SARC_file_ctx* file = (SARC_file_ctx*)io->opaque;
    file->io->destroy(file->io);
    allocator.Free(file);
    allocator.Free(io);
} /* SARC_destroy */

