add_executable(sarc_archiver
  "main.c"
  "archiver_sarc.c"
  "vmem.c"
  "physfs_utils.c"
)

target_link_libraries(sarc_archiver physfs-static)

