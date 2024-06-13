add_library(sarc_archiver STATIC
    archiver_sarc.h
    archiver_sarc.c
    vmem.c
    physfs_utils.c
    sarc_io.c
    zstd_io.c
    logging.c
)

target_link_libraries(sarc_archiver PUBLIC physfs-static zstd)
target_include_directories(sarc_archiver PUBLIC "./")

if (SARC_ARCHIVER_BUILD_TEST)
		add_executable(sarc_archiver_test main.c)
		target_link_libraries(sarc_archiver_test PUBLIC sarc_archiver)
endif()
