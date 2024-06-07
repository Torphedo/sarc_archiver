#include <physfs.h>
#include <string.h>
#define __PHYSICSFS_INTERNAL__
#include <physfs_internal.h>

#include "physfs_utils.h"

typedef struct {
  char **list;
  PHYSFS_uint32 size;
  __PHYSFS_DirTree* tree;
}EnumStringListCallbackData;

static int locateInStringList(const char *str, char **list, PHYSFS_uint32 *pos) {
  PHYSFS_uint32 len = *pos;
  PHYSFS_uint32 half_len;
  PHYSFS_uint32 lo = 0;
  PHYSFS_uint32 middle;
  int cmp;

  while (len > 0) {
    half_len = len >> 1;
    middle = lo + half_len;
    cmp = strcmp(list[middle], str);

    if (cmp == 0) {  /* it's in the list already. */
      return 1;
    }
    else if (cmp > 0) {
      len = half_len;
    }
    else {
      lo = middle + 1;
      len -= half_len + 1;
    } /* else */
  } /* while */

  *pos = lo;
  return 0;
} /* locateInStringList */

static PHYSFS_EnumerateCallbackResult enumFilesCallback(void* data, const char* origdir, const char *fname) {
  callback_data* callback = (callback_data*)data;

  // The plus 2 at the end is important! We need space for both the null terminator and the "/" we insert.
  char* full_path = __PHYSFS_smallAlloc(strlen(origdir) + strlen(fname) + 2);
  if (full_path == NULL) {
    return PHYSFS_ENUM_ERROR;
  }

  // It doesn't want a leading slash, but we need a slash between directories.
  if (origdir[0] == 0) {
    // No containing dir.
    strcpy(full_path, fname);
  } else {
    sprintf(full_path, "%s/%s", origdir, fname);
  }

  PHYSFS_Stat statbuf = {0};
  PHYSFS_stat(full_path, &statbuf);
  if  (statbuf.filetype == PHYSFS_FILETYPE_DIRECTORY) {
    __PHYSFS_DirTreeEnumerate(callback->tree, full_path, enumFilesCallback, full_path, data);
    __PHYSFS_smallFree(full_path);
  }
  else {
    // We've finally got a real file.
    uint32_t size = strlen(full_path) + 1;
    char* target = allocator.Malloc(size);
    if (target == NULL) {
      return PHYSFS_ENUM_ERROR;
    }
    strncpy(target, full_path, size);

    callback->file_list[callback->current] = target;
    callback->current++;
  }

  return PHYSFS_ENUM_OK;
} /* enumFilesCallback */

PHYSFS_EnumerateCallbackResult callback_file_count(void *data, const char *origdir, const char *fname) {
  callback_data* callback = (callback_data*)data;
  // The plus 2 at the end is important! We need space for both the null terminator and the "/" we insert.
  char* full_path = __PHYSFS_smallAlloc(strlen(origdir) + strlen(fname) + 2);
  if (full_path == NULL) {
    return PHYSFS_ENUM_ERROR;
  }

  // It doesn't want a leading slash, but we need a slash between directories.
  if (origdir[0] == 0) {
    // No containing dir.
    strcpy(full_path, fname);
  } else {
    sprintf(full_path, "%s/%s", origdir, fname);
  }

  PHYSFS_Stat statbuf = {0};
  PHYSFS_stat(full_path, &statbuf);
  if  (statbuf.filetype == PHYSFS_FILETYPE_DIRECTORY) {
    __PHYSFS_DirTreeEnumerate(callback->tree, full_path, callback_file_count, full_path, data);
  }
  else {
    // We've finally got a real file.
    callback->file_count++;
  }

  __PHYSFS_smallFree(full_path);
  return PHYSFS_ENUM_OK;
}

char** __PHYSFS_enumerateFilesTree(void* dir_tree, const char *path) {
  __PHYSFS_DirTree* tree = (__PHYSFS_DirTree*) dir_tree;
  callback_data data = {
    .tree = dir_tree,
    .file_count = 0,
    .file_list = 0
  };
  // Get file count first.
  __PHYSFS_DirTreeEnumerate(data.tree, path, callback_file_count, path, &data);
  uint32_t size = (data.file_count + 1) * sizeof(char*);
  data.file_list = allocator.Malloc(size);
  memset(data.file_list, 0x00, size);

  BAIL_IF(!data.file_list, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
  if (!__PHYSFS_DirTreeEnumerate(tree, path, enumFilesCallback, path, &data)) {
    for (PHYSFS_uint32 i = 0; i < data.file_count; i++) {
      allocator.Free(data.file_list[i]);
    }
    allocator.Free(data.file_list);
    return NULL;
  }

  data.file_list[data.file_count] = NULL;
  return data.file_list;
} /* PHYSFS_enumerateFiles */

static const char* module_name_info = "[\033[32mVFS\033[0m]";
static const char* module_name_warn = "[\033[33mVFS\033[0m]";
static const char* module_name_err = "[\033[31mVFS\033[0m]";

bool path_has_extension(const char* path, const char* extension) {
    uint32_t pos = strlen(path);
    uint16_t ext_length = strlen(extension);

    // File extension is longer than input string.
    if (ext_length > pos) {
        return false;
    }
    return (strncmp(&path[pos - ext_length], extension, ext_length) == 0);
}


void mount_archive_recursive(const char* extension, const char* dir, const char* mountpoint) {
// Recursive Archive Mounter
    char** file_list = PHYSFS_enumerateFiles(dir);

    for (char** i = file_list; *i != NULL; i++) {
        if (*i == NULL) {
            printf("%s mount_archive_recursive(): Something has gone terribly wrong with the filesystem.\n", module_name_err);
            PHYSFS_freeList(file_list);
            return;
        }
        if (path_has_extension(*i, extension)) {
            char full_path[512] = {0}; // 512 bytes is enough...right?

            // Get full virtual filesystem path.
            sprintf(full_path, "%s%s%s", dir, PHYSFS_getDirSeparator(), *i);

            printf("%s: Mounting %s at %s\n", module_name_info, full_path, mountpoint);

            // Real search path + / or \ + filename
            sprintf(full_path, "%s%s%s%s", PHYSFS_getRealDir(full_path), dir, PHYSFS_getDirSeparator(), *i);

            // Mount to the current virtual directory.
            uint32_t error = PHYSFS_mount(full_path, mountpoint, true);

            if (error == 0) {
                printf("%s Mount failed. Message: %s\n", module_name_err, PHYSFS_getLastError());
            }
        }
    }
    PHYSFS_freeList(file_list);
} /* mount_archive_recursive */
