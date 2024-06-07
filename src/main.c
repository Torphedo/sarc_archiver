#include <stdio.h>
#include <stdint.h>
#include <malloc.h>

// Linux-only
#include <sys/resource.h>

#include <physfs.h>


#include "archiver_sarc.h"
#include "physfs_utils.h"

const char packname[] = "Armor_012_Upper.pack";

void increase_file_limit() {
    struct rlimit rlim;
    int err = getrlimit(RLIMIT_NOFILE, &rlim);
    if (err < 0) {
        printf("Failed to get rlimit...\n");
        return;
    }
    rlim.rlim_cur = 15100;
    setrlimit(RLIMIT_NOFILE, &rlim);
}

int main(int argc, char** argv) {
  increase_file_limit();

  PHYSFS_init(argv[0]);

  PHYSFS_mount(PHYSFS_getBaseDir(), NULL, true);
  PHYSFS_setWriteDir(PHYSFS_getBaseDir());

  PHYSFS_registerArchiver(&archiver_sarc_default);

  const PHYSFS_ArchiveInfo** i = NULL;
  for (i = PHYSFS_supportedArchiveTypes(); *i != NULL; i++) {
    printf("main(): Supported archive: [%s], which is [%s].\n", (*i)->extension, (*i)->description);
  }

  printf("Mounting all SARC archives...\n");
  mount_archive_recursive(".pack", "data", "/");
  printf("Done.\n");

  char** file_list = PHYSFS_enumerateFiles("/");

  for (char** i = file_list; *i != NULL; i++) {
      if (*i == NULL) {
          break;
      }
      printf("We've got [%s]\n", *i);
  }
  PHYSFS_freeList(file_list);

  PHYSFS_file* test_file = PHYSFS_openWrite("/Component/ArmorParam/Armor_012_Upper.game__component__ArmorParam.bgyml");
  PHYSFS_close(test_file);
}
