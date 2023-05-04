add_executable(sarc_archiver
    "main.c"
    "archiver_sarc.c"
    "custom_stream_standard.c"
)

target_link_libraries(sarc_archiver physfs-static)

