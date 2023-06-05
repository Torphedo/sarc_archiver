#include <stdio.h>
#include <stdint.h>
#include <malloc.h>

#include <physfs.h>

#include "archiver_sarc.h"

const char packname[] = "Armor_012_Upper.pack";

int main(int argc, char** argv) {
  PHYSFS_init(argv[0]);

  PHYSFS_mount(PHYSFS_getBaseDir(), NULL, true);
  PHYSFS_setWriteDir(PHYSFS_getBaseDir());

  PHYSFS_registerArchiver(&archiver_sarc_default);

  const PHYSFS_ArchiveInfo** i = NULL;
  for (i = PHYSFS_supportedArchiveTypes(); *i != NULL; i++) {
    printf("main(): Supported archive: [%s], which is [%s].\n", (*i)->extension, (*i)->description);
  }

  printf("main(): Mounting %s...\n\n", packname);
  PHYSFS_mount(packname, NULL, true);

  PHYSFS_file* test_write = PHYSFS_openWrite("/Component/ArmorParam/Armor_012_Upper.game__component__ArmorParam.bgyml");
  PHYSFS_close(test_write);
}
