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

    rc = PHYSFS_enumerateFiles("/");
    for (char** i = rc; *i != NULL; i++) {
        printf(" * We've got [%s].\n", *i);
    }
    PHYSFS_freeList(rc);

    PHYSFS_file* file = PHYSFS_openRead("/Component/ArmorParam/Armor_012_Upper.game__component__ArmorParam.bgyml");
    uint32_t size = PHYSFS_fileLength(file);

    uint8_t* buffer = calloc(1, size);
    PHYSFS_readBytes(file, buffer, size);
    PHYSFS_close(file);
    FILE* output = fopen("Armor_012_Upper.game__component__ArmorParam.bgyml", "wb");
    fwrite(buffer, size, 1, output);
    fclose(output);
    free(buffer);

    PHYSFS_file* test_write = PHYSFS_openWrite("/Component/ArmorParam/Armor_012_Upper.game__component__ArmorParam.bgyml");
    PHYSFS_close(test_write);
}
