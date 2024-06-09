#include <stdio.h>
#include <stdint.h>
#include <malloc.h>

// Linux-only
#include <sys/resource.h>

#include <physfs.h>


#include "archiver_sarc.h"
#include "physfs_utils.h"
#include "logging.h"

const char packname[] = "Armor_012_Upper.pack";

bool increase_file_limit() {
    struct rlimit rlim;
    int err = getrlimit(RLIMIT_NOFILE, &rlim);
    if (err < 0) {
        LOG_MSG(error, "Failed to get rlimit (code %d)\n", err);
        return false;
    }
    rlim.rlim_cur = 200100;
    err = setrlimit(RLIMIT_NOFILE, &rlim);
    if (err < 0) {
        LOG_MSG(error, "Failed to set rlimit (code %d)\n", err);
        return false;
    }
    return true;
}


int main(int argc, char** argv) {
  enable_win_ansi();
  if (!increase_file_limit()) {
      return 1;
  }

  PHYSFS_init(argv[0]);

  PHYSFS_mount(PHYSFS_getBaseDir(), NULL, true);
  PHYSFS_permitDanglingWriteHandles(1);
  PHYSFS_setWriteDir(PHYSFS_getBaseDir());

  PHYSFS_registerArchiver(&archiver_sarc_default);

  const PHYSFS_ArchiveInfo** i = NULL;
  LOG_MSG(info, "The supported archive formats are:\n");
  for (i = PHYSFS_supportedArchiveTypes(); *i != NULL; i++) {
        printf("\t[%s] (%s).\n", (*i)->extension, (*i)->description);
  }

  LOG_MSG(info, "Mounting all SARC archives...\n");
  mount_archive_recursive(".pack", "data", "/");
  LOG_MSG(info, "Done.\n");

  char** file_list = PHYSFS_enumerateFiles("/");
  for (char** i = file_list; *i != NULL; i++) {
      if (*i == NULL) {
          break;
      }
      LOG_MSG(info, "We've got [%s]\n", *i);
  }
  PHYSFS_freeList(file_list);

  char* files[] = {
      "Component/ArmorParam/Armor_012_Upper.game__component__ArmorParam.bgyml",
  };

  for (unsigned int i = 0; i < sizeof(files) / sizeof(*files); i++) {
      int name_count = 0;
      const char** names;
      PHYSFS_getRealDirs(files[i], &names, &name_count);

      for (int j = 0; j < name_count; j++) {
          LOG_MSG(debug, "The file is part of %s\n", names[j]);
          // Create archive
          PHYSFS_file* arc = PHYSFS_openWrite(names[j]);
          PHYSFS_close(arc);

          // Write dir juggling
          char* old_write_dir_temp = PHYSFS_getWriteDir();
          char* old_write_dir = allocator.Malloc(strlen(old_write_dir_temp) + 1);
          if (old_write_dir == NULL) {
              LOG_MSG(error, "Alloc failed for old write dir!\n");
              break;
          }
          strcpy(old_write_dir, old_write_dir_temp);

          // Set the new write dir to our archive
          PHYSFS_setWriteDir(names[j]);

          PHYSFS_mkdir("Component/ArmorParam");

          PHYSFS_file* file = PHYSFS_openWrite(files[i]);

          // Write dir juggling
          PHYSFS_setWriteDir(old_write_dir);
          allocator.Free(old_write_dir);

          PHYSFS_close(file);
      }

      allocator.Free(names);
  }

  LOG_MSG(info, "VFS shutdown\n");
  PHYSFS_deinit();

}
