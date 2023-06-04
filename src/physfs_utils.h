#pragma once
#include <stdint.h>

#include <physfs.h>
#define __PHYSICSFS_INTERNAL__
#include <physfs_internal.h>

typedef struct {
  char** file_list;
  uint32_t file_count;
  uint32_t current;
  __PHYSFS_DirTree* tree;
}callback_data;

PHYSFS_EnumerateCallbackResult callback_file_count(void *data, const char *origdir, const char *fname);
char** __PHYSFS_enumerateFilesTree(void* dir_tree, const char *path);
