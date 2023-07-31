add_library(sarc_archiver STATIC "archiver_sarc.h" "archiver_sarc.c" "vmem.h" "vmem.c" "physfs_utils.h" "physfs_utils.c")

target_link_libraries(sarc_archiver PUBLIC physfs-static)
target_include_directories(sarc_archiver PUBLIC "./")

if (SARC_ARCHIVER_BUILD_TEST)
add_executable(sarc_archiver_test "main.c")
target_link_libraries(sarc_archiver_test PUBLIC sarc_archiver)
endif()