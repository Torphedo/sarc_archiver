#include <zstd.h>
#include <common/zstd_internal.h>
#include <physfs.h>
#define __PHYSICSFS_INTERNAL__
#include <physfs_internal.h>

#include "zstd_io.h"
#include "physfs_utils.h"

#include "int.h"
#include "logging.h"

ZSTD_DDict* dict_buffers[3];
typedef struct {
    u32 read_limit;
    u32 read_count;

    PHYSFS_Io* io;
    // Stream object for decompression
    ZSTD_DCtx* dstream;

    // If we treated the file as an array of decompression buffers, this is our
    // index. Current position = dbuf_idx * sizeof(dbuf) + dpos
    u32 dbuf_idx;

    size_t dpos;
    u8* dbuf;

    u8* in_buf;
    size_t in_buf_idx;
    size_t in_pos;
}zstd_ctx;

enum {
    OUT_SIZE = ZSTD_BLOCKSIZE_MAX,
    IN_SIZE = ZSTD_BLOCKSIZE_MAX + ZSTD_BLOCKHEADERSIZE,
    READLIMIT_DEFAULT = 10,
};

void zstd_io_add_dict(const char* path) {
    for (u32 i = 0; i < ARRAY_SIZE(dict_buffers); i++) {
        // Skip elements that are already filled
        if (dict_buffers[i] != NULL) {
            continue;
        }
        // Load the file now that we've found an empty space
        PHYSFS_File* file = PHYSFS_openRead(path);
        if (file == NULL) {
            return;
        }
        u32 size = PHYSFS_fileLength(file);
        u8* buf = allocator.Malloc(size);
        if (buf == NULL) {
            return;
        }
        PHYSFS_readBytes(file, buf, size);
        PHYSFS_close(file);

        // Add the dictionary to our list
        dict_buffers[i] = ZSTD_createDDict_byReference(buf, size);
        break;
    }
}

bool zstd_decompress_block(zstd_ctx* ctx) {
    // If we need more data but our buffers were freed, we need to re-alloc
    if (ctx->dbuf == NULL) {
        LOG_MSG(debug, "Had to alloc temp buffer.\n");
        ctx->dbuf = allocator.Malloc(OUT_SIZE);
        if (ctx == NULL) {
            return 0;
        }
    }
    if (ctx->in_buf == NULL) {
        LOG_MSG(debug, "Had to alloc temp buffer.\n");
        ctx->in_buf = allocator.Malloc(IN_SIZE);
        if (ctx == NULL) {
            return 0;
        }
        // We need to get back the input data we freed
        for (u32 i = 0; i < ctx->in_buf_idx; i++) {
            ctx->io->read(ctx->io, (void*)ctx->in_buf, IN_SIZE);
        }
    }

    ctx->dbuf_idx++;
    ctx->dpos = 0;
    size_t rc = 1;
    while (rc != 0) {
        bool input_remaining = (ctx->in_pos < IN_SIZE) && ctx->in_pos > 0;
        if (!input_remaining) {
            ctx->io->read(ctx->io, (void*)ctx->in_buf, IN_SIZE);
            ctx->in_buf_idx++;
        }

        rc = ZSTD_decompressStream_simpleArgs(ctx->dstream, ctx->dbuf, OUT_SIZE, &ctx->dpos, ctx->in_buf, IN_SIZE, &ctx->in_pos);
        if (ZSTD_isError(rc)) {
            ZSTD_ErrorCode err = ZSTD_getErrorCode(rc);
            if (err == ZSTD_error_noForwardProgress_destFull) {
                ctx->dpos = 0;
                break;
            }
            LOG_MSG(error, "ZSTD error code %d [%s]\n", err, ZSTD_getErrorString(err));
            return false;
        }

        bool output_flushed = (ctx->dpos < OUT_SIZE);
        if (output_flushed) {
            break;
        }
    }
    ctx->dpos = 0;
    return true;
}

bool zstd_ctx_init(zstd_ctx* ctx) {
    // After this many reads, our decompression buffers will be freed and have
    // to be re-allocated next time they're needed.
    ctx->read_limit = READLIMIT_DEFAULT;

    // Setup compression & register dictionaries
    ctx->dstream = ZSTD_createDStream();
    ZSTD_initDStream(ctx->dstream);
    ZSTD_DCtx_setParameter(ctx->dstream, ZSTD_d_refMultipleDDicts, ZSTD_rmd_refMultipleDDicts);
    for (u32 i = 0; i < ARRAY_SIZE(dict_buffers); i++) {
        ZSTD_DDict* dict = dict_buffers[i];
        if (dict != NULL) {
            size_t rc = ZSTD_DCtx_refDDict(ctx->dstream, dict);
            if (ZSTD_isError(rc)) {
                ZSTD_ErrorCode err = ZSTD_getErrorCode(rc);
                LOG_MSG(error, "ZSTD error code %d [%s]\n", err, ZSTD_getErrorString(err));
            }
        }
    }

    // Alloc our decompression buffers
    ctx->dbuf = allocator.Malloc(OUT_SIZE);
    ctx->in_buf = allocator.Malloc(IN_SIZE);

    // Decompress the first chunk so we have data to work with already
    zstd_decompress_block(ctx);
    return true;
}

// PHYSFS_Io implementation for ZSTD files

PHYSFS_sint64 zstd_read(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 len) {
    zstd_ctx* ctx = (zstd_ctx*)io->opaque;
    if (ctx->dbuf == NULL) {
        LOG_MSG(debug, "Had to alloc temp buffer.\n");
        ctx->dbuf = allocator.Malloc(OUT_SIZE);
        if (ctx == NULL) {
            return 0;
        }
    }
    u32 dest_pos = 0;

    // Keep reading until the entire length is read
    while (1) {
        if (ctx->dpos > OUT_SIZE) {
            LOG_MSG(warning, "Invalid dbuf position 0x%x\n", ctx->dpos);
            return 0;
        }
        // Copy the entire length, or whatever's left in the streaming buffer
        s32 remaining = OUT_SIZE - ctx->dpos;
        u32 size = MIN(remaining, len);
        memcpy((u8*)buffer + dest_pos, (u8*)ctx->dbuf + ctx->dpos, size);

        // Update streaming buffer & other state
        len -= size;
        ctx->dpos += size;
        dest_pos += size;

        if (len == 0) {
            // We had enough in the buffer to do the read, and can exit
            break;
        }

        // We haven't fulfilled the read yet, stream in another block.
        zstd_decompress_block(ctx);
    }

    ctx->read_count++;
    if (ctx->read_count == ctx->read_limit) {
        allocator.Free(ctx->dbuf);
        allocator.Free(ctx->in_buf);
        ctx->dbuf = NULL;
        ctx->in_buf = NULL;
    }
    return len;
}

PHYSFS_sint64 zstd_write(PHYSFS_Io *io, const void* buf, PHYSFS_uint64 len){
    return 0;
}

int zstd_seek(PHYSFS_Io *io, PHYSFS_uint64 offset) {
    zstd_ctx* ctx = (zstd_ctx*)io->opaque;

    u64 block_pos = (ctx->dbuf_idx - 1) * OUT_SIZE;
    // If the destination is in range of our decompressed buffer, just use that
    if (block_pos < offset && offset < block_pos + OUT_SIZE) {
        ctx->dpos = offset - block_pos;
        return 1;
    }

    if (offset < block_pos) {
        // The target is behind the current position, we have to reset the
        // stream and then seek forward to hit it.
        ZSTD_DCtx_reset(ctx->dstream, ZSTD_reset_session_only);
        ctx->io->seek(ctx->io, 0);
        ctx->dbuf_idx = 0;
        zstd_decompress_block(ctx);
        block_pos = (ctx->dbuf_idx - 1) * OUT_SIZE;
    }

    // Decompress blocks until the target offset is between the current decompressed block and the next
    while (offset > block_pos && offset > block_pos + OUT_SIZE) {
        zstd_decompress_block(ctx);
        block_pos = (ctx->dbuf_idx - 1) * OUT_SIZE;
    }
    ctx->dpos = offset - block_pos;

    return 1;
}

PHYSFS_sint64 zstd_tell(PHYSFS_Io *io) {
    zstd_ctx* ctx = (zstd_ctx*)io->opaque;
    // We subtract one because the idx is incremented on every decompression (including the first)
    u64 block_pos = (ctx->dbuf_idx - 1) * OUT_SIZE;
    return block_pos + ctx->dpos;
}

PHYSFS_sint64 zstd_length(PHYSFS_Io *io) {
    zstd_ctx* ctx = (zstd_ctx*)io->opaque;
    ZSTD_DCtx_reset(ctx->dstream,  ZSTD_reset_session_only);

    u64 size = 0;
    while (zstd_decompress_block(ctx)) {
        size += OUT_SIZE;
    }

    return size;
}

void zstd_set_io_file_count(PHYSFS_Io* io, u32 count) {
    zstd_ctx* ctx = (zstd_ctx*)io->opaque;
    // This means it's *probably* a ZSTD context and not something random.
    if (ctx->read_limit == READLIMIT_DEFAULT && ctx->io != NULL) {
        ctx->read_limit = 3 + count;
    }
}

PHYSFS_Io* zstd_wrap_io(PHYSFS_Io* io) {
    PHYSFS_Io* out = allocator.Malloc(sizeof(*out));
    zstd_ctx* new_ctx = allocator.Malloc(sizeof(*new_ctx));
    if (out == NULL || new_ctx == NULL) {
        allocator.Free(out);
        allocator.Free(new_ctx);
        return NULL;
    }
    *out = ZSTD_Io;
    memset(new_ctx, 0x00, sizeof(*new_ctx));

    // Setup our context for streaming decompression, and to wrap the other IO
    new_ctx->io = io;
    zstd_ctx_init(new_ctx);
    out->opaque = new_ctx;

    return out;
}
PHYSFS_Io *zstd_duplicate(PHYSFS_Io *io) {
    zstd_ctx* old_ctx = (zstd_ctx*)io->opaque;
    PHYSFS_Io* out = allocator.Malloc(sizeof(*out));
    zstd_ctx* new_ctx = allocator.Malloc(sizeof(*new_ctx));
    if (out == NULL || new_ctx == NULL) {
        allocator.Free(out);
        allocator.Free(new_ctx);
        return NULL;
    }
    *out = ZSTD_Io;
    memset(new_ctx, 0x00, sizeof(*new_ctx));

    new_ctx->io = old_ctx->io->duplicate(old_ctx->io);

    zstd_ctx_init(new_ctx);
    out->opaque = new_ctx;

    return out;
}

int zstd_flush(PHYSFS_Io *io) {
    return 1;
}

void zstd_destroy(PHYSFS_Io *io) {
    zstd_ctx* ctx = (zstd_ctx*)io->opaque;
    ZSTD_freeDStream(ctx->dstream);
    allocator.Free(ctx->dbuf);
    allocator.Free(ctx);
    return;
}

