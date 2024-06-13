#include <zstd.h>
#include <common/zstd_internal.h>
#include <physfs.h>
#define __PHYSICSFS_INTERNAL__
#include <physfs_internal.h>

#include "archiver_sarc_internal.h"
#include "zstd_io.h"
#include "physfs_utils.h"

#include "int.h"
#include "logging.h"

ZSTD_DDict* dict_buffers[3];
typedef struct {
    u32 magic;
    PHYSFS_Io* io;

    // Stream object for decompression
    ZSTD_DCtx* dstream;
    // If we treated the file as an array of decompression buffers, this is our
    // index. Current position = dbuf_idx * sizeof(d_dst) + dbuf.pos
    u32 dbuf_idx;

    // Buffer object and its backing "destination" buffer for decompression.
    // dbuf.dst should always be d_dst.
    ZSTD_outBuffer dbuf;
    u8 d_dst[ZSTD_BLOCKSIZE_MAX];

    u8 in_src[ZSTD_BLOCKSIZE_MAX + ZSTD_BLOCKHEADERSIZE];
    ZSTD_inBuffer in_buf;
}zstd_ctx;

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

bool zstd_ctx_init(zstd_ctx* ctx) {
    ctx->dstream = ZSTD_createDStream();
    ctx->magic = ZSTD_MAGICNUMBER;
    ctx->dbuf.pos = 0;
    ctx->dbuf.size = 0;
    ctx->dbuf_idx = 0;
    ctx->dbuf.dst = ctx->d_dst;
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

    ctx->in_buf.pos = 0;
    ctx->in_buf.src = ctx->in_src;
    ctx->in_buf.size = sizeof(ctx->in_src);

    return true;
}

bool zstd_decompress_block(zstd_ctx* ctx) {
    ctx->dbuf_idx++;
    ctx->dbuf.size = sizeof(ctx->d_dst);
    ctx->dbuf.pos = 0;
    size_t rc = 1;
    while (rc != 0) {
        bool input_remaining = (ctx->in_buf.pos < ctx->in_buf.size) && ctx->in_buf.pos > 0;
        if (!input_remaining) {
            ctx->io->read(ctx->io, (void*)ctx->in_buf.src, ctx->in_buf.size);
        }
        rc = ZSTD_decompressStream(ctx->dstream, &ctx->dbuf, &ctx->in_buf);
        if (ZSTD_isError(rc)) {
            ZSTD_ErrorCode err = ZSTD_getErrorCode(rc);
            if (err == ZSTD_error_noForwardProgress_destFull) {
                ctx->dbuf.pos = 0;
                break;
            }
            LOG_MSG(error, "ZSTD error code %d [%s]\n", err, ZSTD_getErrorString(err));
            return false;
        }

        bool output_flushed = (ctx->dbuf.pos < ctx->dbuf.size);
        if (output_flushed) {
            break;
        }
    }
    ctx->dbuf.pos = 0;
    return true;
}

// PHYSFS_Io implementation for ZSTD files

PHYSFS_sint64 zstd_read(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 len) {
    zstd_ctx* ctx = (zstd_ctx*)io->opaque;
    if (ctx == NULL) {
        ctx = allocator.Malloc(sizeof(*ctx));
        if (ctx == NULL) {
            return 0;
        }
        zstd_ctx_init(ctx);
    }
    u32 dest_pos = 0;

    // Keep reading until the entire length is read
    while (1) {
        if (ctx->dbuf.pos > ctx->dbuf.size) {
            LOG_MSG(warning, "Invalid dbuf position 0x%x\n", ctx->dbuf.pos);
            break;
        }
        // Copy the entire length, or whatever's left in the streaming buffer
        s32 remaining = ctx->dbuf.size - ctx->dbuf.pos;
        u32 size = MIN(remaining, len);
        memcpy((u8*)buffer + dest_pos, (u8*)ctx->dbuf.dst + ctx->dbuf.pos, size);

        // Update streaming buffer & other state
        len -= size;
        ctx->dbuf.pos += size;
        dest_pos += size;

        if (len == 0) {
            // We had enough in the buffer to do the read, and can exit
            return size;
        }

        // We haven't fulfilled the read yet, stream in another block.
        zstd_decompress_block(ctx);
    }
}

PHYSFS_sint64 zstd_write(PHYSFS_Io *io, const void* buf, PHYSFS_uint64 len){
    return 0;
}

int zstd_seek(PHYSFS_Io *io, PHYSFS_uint64 offset) {
    zstd_ctx* ctx = (zstd_ctx*)io->opaque;
    if (ctx->dbuf_idx == 0) {
        // We need to do a read first as part of our setup.
        zstd_decompress_block(ctx);
    }
    u64 block_pos = (ctx->dbuf_idx - 1) * sizeof(ctx->d_dst);
    // If the destination is in range of our decompressed buffer, just use that
    if (block_pos < offset && offset < block_pos + sizeof(ctx->d_dst)) {
        ctx->dbuf.pos = offset - block_pos;
        return 1;
    }

    if (offset < block_pos) {
        // The target is behind the current position, we have to reset the
        // stream and then seek forward to hit it.
        ZSTD_DCtx_reset(ctx->dstream, ZSTD_reset_session_only);
        ctx->io->seek(ctx->io, 0);
        ctx->dbuf_idx = 0;
        zstd_decompress_block(ctx);
        block_pos = (ctx->dbuf_idx - 1) * sizeof(ctx->d_dst);
    }

    // Decompress blocks until the target offset is between the current decompressed block and the next
    while (offset > block_pos && offset > block_pos + sizeof(ctx->d_dst)) {
        zstd_decompress_block(ctx);
        block_pos = (ctx->dbuf_idx - 1) * sizeof(ctx->d_dst);
    }
    ctx->dbuf.pos = offset - block_pos;

    return 1;
}

PHYSFS_sint64 zstd_tell(PHYSFS_Io *io) {
    zstd_ctx* ctx = (zstd_ctx*)io->opaque;
    // We subtract one because the idx is incremented on every decompression (including the first)
    u64 block_pos = (ctx->dbuf_idx - 1) * sizeof(ctx->d_dst);
    return block_pos + ctx->dbuf.pos;
}

PHYSFS_sint64 zstd_length(PHYSFS_Io *io) {
    zstd_ctx* ctx = (zstd_ctx*)io->opaque;
    ZSTD_DCtx_reset(ctx->dstream,  ZSTD_reset_session_only);

    u64 size = 0;
    while (zstd_decompress_block(ctx)) {
        size += sizeof(ctx->d_dst);
    }

    return size;
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

    // Setup our context for streaming decompression, and to wrap the other IO
    zstd_ctx_init(new_ctx);
    new_ctx->io = io;
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
    allocator.Free(ctx);
    return;
}

