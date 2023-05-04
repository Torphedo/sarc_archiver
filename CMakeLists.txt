cmake_minimum_required(VERSION 3.16)

project(sarc_archiver LANGUAGES C)

set(CMAKE_C_STANDARD 99)

# Build PhysicsFS without docs, to reduce build time.
set(PHYSFS_BUILD_DOCS FALSE)
set(PHYSFS_BUILD_STATIC TRUE)
set(PHYSFS_BUILD_SHARED FALSE)
set(PHYSFS_BUILD_TEST FALSE)

# Disable all the custom archive formats we don't need
set(PHYSFS_ARCHIVE_ZIP FALSE)
set(PHYSFS_ARCHIVE_7Z FALSE)
set(PHYSFS_ARCHIVE_GRP FALSE)
set(PHYSFS_ARCHIVE_WAD FALSE)
set(PHYSFS_ARCHIVE_MVL FALSE)
set(PHYSFS_ARCHIVE_HOG FALSE)
set(PHYSFS_ARCHIVE_SLB FALSE)
set(PHYSFS_ARCHIVE_VDF FALSE)
set(PHYSFS_ARCHIVE_QPAK FALSE)
set(PHYSFS_ARCHIVE_ISO9660 FALSE)

add_subdirectory(src)
add_subdirectory(ext/physfs)

file(COPY "data/" DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/src)

