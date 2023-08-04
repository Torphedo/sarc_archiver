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
#include "physfs_utils.h"
#include "vmem.h"

static const char* module_name_info = "[\033[32mVFS::SARC\033[0m]";
static const char* module_name_warn = "[\033[33mVFS::SARC\033[0m]";
static const char* module_name_err = "[\033[31mVFS::SARC\033[0m]";

// TODO: See if we can track the number of open write handles, then rebuild the
// SARC and turn it back into a normal read-only archive? It looks like calling
// PHYSFS_close() may call our io->flush() which will let us know when a write
// handle is closed.
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
  uint32_t open_write_handles; // The number of write handles currently open to this archive
  char* arc_filename;
}SARCinfo;

typedef struct {
  __PHYSFS_DirTreeEntry tree;
  PHYSFS_uint64 startPos;
  PHYSFS_uint64 size;
  uintptr_t data_ptr; // Files open for write will store a pointer here instead of an offset.
}SARCentry;

typedef struct {
  PHYSFS_Io *io;
  SARCentry *entry;
  SARCinfo* arc_info;
  PHYSFS_uint32 curPos;
}SARCfileinfo;

typedef struct {
  char **list;
  PHYSFS_uint32 size;
  PHYSFS_ErrorCode errcode;
}EnumStringListCallbackData;


// TODO: Call SARC_flush() here.
void SARC_closeArchive(void *opaque) {
  SARCinfo *info = ((SARCinfo *) opaque);
  if (info) {
    __PHYSFS_DirTreeDeinit(&info->tree);

    if (info->io) {
      info->io->destroy(info->io);
    }

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

  if (!finfo->io->seek(finfo->io, entry->startPos)) {
    goto SARC_openRead_failed;
  }

  finfo->curPos = 0;
  finfo->entry = entry;
  
  // Give IO our archiver info
  finfo->arc_info = opaque;

  // Set SARC_Io as the I/O handler for this archiver
  memcpy(retval, &SARC_Io, sizeof (*retval));
  retval->opaque = finfo;
  return retval;

SARC_openRead_failed:
  if (finfo != NULL) {
    if (finfo->io != NULL) {
      finfo->io->destroy(finfo->io);
    }
    allocator.Free(finfo);
  } /* if */

  if (retval != NULL) {
    allocator.Free(retval);
  }

  return NULL;
} /* SARC_openRead */

// Copy all file contents to newly allocated buffers
PHYSFS_EnumerateCallbackResult callback_copy_files(void *data, const char *origdir, const char *fname) {
  SARCinfo* info = (SARCinfo*)data;
  char* full_path = __PHYSFS_smallAlloc(strlen(origdir) + strlen(fname) + 1);
  if (full_path == NULL) {
    return PHYSFS_ENUM_ERROR;
  }

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
    __PHYSFS_DirTreeEnumerate(&info->tree, full_path, callback_copy_files, full_path, data);
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
  SARCinfo* info = (SARCinfo*) opaque;

  if (findEntry(info, name) == NULL) {
     // File doesn't exist, early exit
     return NULL;
  }

  // Copy file data to their own buffers for more expansion
  __PHYSFS_DirTree* tree = (__PHYSFS_DirTree *) &info->tree;
  __PHYSFS_DirTreeEnumerate(tree, "", callback_copy_files, "", opaque);

  info->open_write_handles++;

  SARCfileinfo* file_info = allocator.Malloc(sizeof(SARCfileinfo));
  if (file_info == NULL) {
    printf("%s SARC_openWrite(): Failed to allocate %li bytes for SARCfileinfo.\n", module_name_err, sizeof(SARCfileinfo));
    return NULL;
  }
  else {
    file_info->curPos = 0;
    file_info->entry = findEntry(info, name);
    file_info->io = info->io->duplicate(info->io);
    file_info->arc_info = opaque;
  }

  PHYSFS_Io* handle = allocator.Malloc(sizeof(PHYSFS_Io));
  if (handle == NULL) {
    printf("%s SARC_openWrite(): Failed to allocate %li bytes for PHYSFS_Io return value.\n", module_name_err, sizeof(PHYSFS_Io));
  }
  else {
    // Use SARC_Io as our I/O handler.
    memcpy(handle, &SARC_Io, sizeof(*handle));
    handle->opaque = file_info;
  }
  return handle;
} /* SARC_openWrite */

PHYSFS_Io *SARC_openAppend(void *opaque, const char *name) {
  PHYSFS_Io* io = SARC_openWrite(opaque, name);
  io->seek(io, io->length(io)); // Move position to end of file
  return io;
} /* SARC_openAppend */

void rebuild_sarc(PHYSFS_Io* io) {
  SARCfileinfo *finfo = (SARCfileinfo *) io->opaque;

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
  char* name = finfo->arc_info->arc_filename;
  PHYSFS_File* arc = PHYSFS_openWrite(name);
  if (arc == NULL) {
    printf("%s rebuild_sarc(): Failed to open SARC file %s", module_name_err, name);
    return;
  }

  // We'll write these later, skip for now.
  PHYSFS_seek(arc, sizeof(header) + sizeof(sfat_header));

  __PHYSFS_DirTree* tree = &finfo->arc_info->tree;
  char** file_list_unsorted = __PHYSFS_enumerateFilesTree(tree, "");
  
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
  PHYSFS_seek(arc, PHYSFS_tell(arc) + (sfat_header.node_count * sizeof(sarc_sfat_node)));

  // Write SFNT
  PHYSFS_writeBytes(arc, &sfnt_header, sizeof(sfnt_header));
  uint32_t filename_pos = PHYSFS_tell(arc);
  const uint32_t filename_start = filename_pos;

  for (char** i = file_list; *i != NULL; i++) {
    PHYSFS_seek(arc, PHYSFS_tell(arc) + strlen(*i) + 1);
    while((PHYSFS_tell(arc) % 4) != 0) {
      PHYSFS_seek(arc, PHYSFS_tell(arc) + 1);
    }
  }

  header.data_offset = PHYSFS_tell(arc); // Now we know where files should start.
  // This tracks where we are while writing files.
  uint32_t file_write_pos = header.data_offset;

  // Time to write SFAT and SFNT filenames.
  PHYSFS_seek(arc, sizeof(header)); // Jump to SFAT header location.
  PHYSFS_writeBytes(arc, &sfat_header, sizeof(sfat_header));
  for (char** i = file_list; *i != NULL; i++) {
    SARCentry* entry = findEntry(finfo->arc_info, *i);

    sarc_sfat_node node = {
      .filename_hash = sarc_filename_hash(*i, strlen(*i), sfat_header.hash_key),
      .enable_offset = 0x0100,
      .filename_offset = (filename_pos - filename_start) / 4,
      .file_start_offset = file_write_pos - header.data_offset,
      .file_end_offset = file_write_pos + entry->size - header.data_offset
    };
    uint32_t cur_pos = PHYSFS_tell(arc); // Save our spot
    // Write the file data.
    PHYSFS_seek(arc, file_write_pos);
    if ((void*)entry->data_ptr == NULL) {
      printf("%s rebuild_sarc(): invalid file data pointer!\n", module_name_err);
      return;
    }
    PHYSFS_writeBytes(arc, (void*)entry->data_ptr, entry->size);
    // Update our file write position and align to 4 byte boundary
    file_write_pos = PHYSFS_tell(arc);
    while((file_write_pos % 8) != 0) {
      file_write_pos++;
    }

    PHYSFS_seek(arc, filename_pos);
    PHYSFS_writeBytes(arc, *i, strlen(*i)); // Write filenames
    // Jump to the next 4-byte alignment boundary.
    while(((PHYSFS_tell(arc) + 1) % 4) != 0) {
      PHYSFS_seek(arc, PHYSFS_tell(arc) + 1); // Jump forward 1 byte
    }
    filename_pos = PHYSFS_tell(arc) + 1;

    // Jump back to where we were, and write the SFAT node.
    PHYSFS_seek(arc, cur_pos);
    PHYSFS_writeBytes(arc, &node, sizeof(node));
  }
  header.archive_size = file_write_pos;
  PHYSFS_seek(arc, 0);
  PHYSFS_writeBytes(arc, &header, sizeof(header));
  
  PHYSFS_freeList(file_list);
}

// Rebuild archive and write to disk.
void close_write_handle(PHYSFS_Io* io) {
  SARCinfo* info = io->opaque;
  info->open_write_handles--;
  rebuild_sarc(io);
}


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

  if (!forWriting) {
      static const char magic[] = "SARC";
      sarc_header header = { 0 };
      io->read(io, &header, sizeof(header));

      if (strncmp((char*)&header.magic, magic, 4) != 0) {
          BAIL(PHYSFS_ERR_UNSUPPORTED, NULL);
      }
      // Claim the archive, because it's probably a valid SARC
      *claimed = 1;

      sarc_sfat_header sfat_header = { 0 };
      io->read(io, &sfat_header, sizeof(sfat_header));

      SARCinfo* archive = SARC_init_archive(io);
      BAIL_IF_ERRPASS(!archive, NULL);

      archive->arc_filename = allocator.Malloc(strlen(name) + 1);
      strcpy(archive->arc_filename, name);

      SARC_loadEntries(io, sfat_header.node_count, header.data_offset, archive);
      return archive;
  }
  else {
      // Claim the archive, because it's gonna be a valid SARC
      *claimed = 1;

      static const char magic[] = "SARC";
      sarc_header header = { .header_size = 0x14, .byte_order_mark = 0xFEFF, .archive_size = 0x0, .data_offset = 0x48, .version = 0x100, .reserved = 0x00 };
      memcpy(&header.magic, &magic, 4);
      io->write(io, &header, sizeof(header));

      static const char sfat_magic[] = "SFAT";
      sarc_sfat_header sfat_header = { .header_size = 0xC, .node_count = 0x0, .hash_key = 0x65 };
      memcpy(&sfat_header.magic, &sfat_magic, 4);
      io->write(io, &sfat_header, sizeof(sfat_header));

      SARCinfo* archive = SARC_init_archive(io);
      BAIL_IF_ERRPASS(!archive, NULL);

      archive->arc_filename = allocator.Malloc(strlen(name) + 1);
      strcpy(archive->arc_filename, name);

      SARC_loadEntries(io, sfat_header.node_count, header.data_offset, archive);
      return archive;
  }
}


// PHYSFS_Io implementation for SARC

PHYSFS_sint64 SARC_read(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 len) {
  SARCfileinfo *finfo = (SARCfileinfo *) io->opaque;
  const SARCentry *entry = finfo->entry;
  const PHYSFS_uint64 bytesLeft = (PHYSFS_uint64)(entry->size - finfo->curPos);
  PHYSFS_sint64 rc;

  if (bytesLeft < len) {
    len = bytesLeft;
  }

  rc = finfo->io->read(finfo->io, buffer, len);
  if (rc > 0) {
    finfo->curPos += (PHYSFS_uint32) rc;
  }

  return rc;
} /* SARC_read */

PHYSFS_sint64 SARC_write(PHYSFS_Io *io, const void* buf, PHYSFS_uint64 len) {
  SARCfileinfo* finfo = (SARCfileinfo*) io->opaque;
  SARCentry* entry = finfo->entry;

  // Sanity checks.
  if (buf == NULL) {
    printf("%s SARC_write(): Trying to write %lli bytes from a nullptr!\n", module_name_err, len);
    return -1;
  }
  // Most writes are under 4MiB... warn for unusally large individual writes,
  // in case someone passed in a bad value.
  if (len > 0x400000) {
    // Sorry for the extremely long line.
    printf("%s SARC_write(): Writing %lli bytes from a buffer at 0x%p. Writing will proceed normally, this is just a friendly alert that you might've passed a bad value.\n", module_name_warn, len, buf);
  }

  if ((void*)entry->data_ptr != NULL) {
    // Not to jinx myself, but this should NEVER happen because opening a write
    // handle automatically sets this up.
    printf("%s SARC_write(): Tried to write to a file that isn't set up for writing.\n", module_name_err);
    BAIL(PHYSFS_ERR_READ_ONLY, -1);
  }
  
  // Since files open for writing are only in memory until they're flushed by
  // closing the handle, we just do a memcpy.
  if (finfo->curPos + len >= entry->size) {
    // We're out of space, time to expand. Expand enough to fit this entire
    // write plus 500 bytes.
    virtual_commit((void*)entry->data_ptr, entry->size + len + 500);

  }

  memcpy((void*)entry->data_ptr, buf, len);
  return 0;
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
  if (rc) {
    finfo->curPos = (PHYSFS_uint32) offset;
  }

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

// TODO: Make this decrement write handle counter and rebuild SARC then flush to
// disk. If there are no more open write handles, free the individual file
// buffers and make it a normal read-only archive again. This function is only
// called when closing a write handle or shutting down PhysicsFS.
int SARC_flush(PHYSFS_Io *io) {
  close_write_handle(io);
  return 1;
}

void SARC_destroy(PHYSFS_Io *io) {
  SARCfileinfo *finfo = (SARCfileinfo *) io->opaque;
  finfo->io->destroy(finfo->io);
  allocator.Free(finfo);
  allocator.Free(io);
} /* SARC_destroy */

