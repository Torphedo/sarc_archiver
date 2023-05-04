#include <stdio.h>

#include <physfs.h>

#include "archiver_sarc.h"

const char packname[] = "Armor_012_Upper.pack";

int main(int argc, char** argv) {
    PHYSFS_init(argv[0]);

    PHYSFS_mount(PHYSFS_getBaseDir(), NULL, true);
    PHYSFS_setWriteDir(PHYSFS_getBaseDir());

    PHYSFS_registerArchiver(&archiver_sarc_default);

    for (const PHYSFS_ArchiveInfo** i = PHYSFS_supportedArchiveTypes(); *i != NULL; i++) {
        printf("Supported archive: [%s], which is [%s].\n",
                (*i)->extension, (*i)->description);
    }

    char** rc = PHYSFS_enumerateFiles("/");
    for (char** i = rc; *i != NULL; i++) {
        printf(" * We've got [%s].\n", *i);
    }

    printf("Mounting %s...\n\n", packname);
    PHYSFS_mount(packname, NULL, true);

    rc = PHYSFS_enumerateFiles("/Component/");
    for (char** i = rc; *i != NULL; i++) {
        printf(" * We've got [%s].\n", *i);
    }
    PHYSFS_freeList(rc);
}
