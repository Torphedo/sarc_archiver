cmake_minimum_required(VERSION 3.16)

project(sarc_archiver LANGUAGES C)

set(CMAKE_C_STANDARD 99)

# Build PhysicsFS without docs, to reduce build time.
set(PHYSFS_BUILD_DOCS FALSE CACHE BOOL "" FORCE)
set(PHYSFS_BUILD_STATIC TRUE CACHE BOOL "" FORCE)
set(PHYSFS_BUILD_SHARED FALSE CACHE BOOL "" FORCE)
set(PHYSFS_BUILD_TEST FALSE CACHE BOOL "" FORCE)

# Disable all the custom archive formats we don't need
set(PHYSFS_ARCHIVE_ZIP FALSE CACHE BOOL "" FORCE)
set(PHYSFS_ARCHIVE_7Z FALSE CACHE BOOL "" FORCE)
set(PHYSFS_ARCHIVE_GRP FALSE CACHE BOOL "" FORCE)
set(PHYSFS_ARCHIVE_WAD FALSE CACHE BOOL "" FORCE)
set(PHYSFS_ARCHIVE_MVL FALSE CACHE BOOL "" FORCE)
set(PHYSFS_ARCHIVE_HOG FALSE CACHE BOOL "" FORCE)
set(PHYSFS_ARCHIVE_SLB FALSE CACHE BOOL "" FORCE)
set(PHYSFS_ARCHIVE_VDF FALSE CACHE BOOL "" FORCE)
set(PHYSFS_ARCHIVE_QPAK FALSE CACHE BOOL "" FORCE)
set(PHYSFS_ARCHIVE_ISO9660 FALSE CACHE BOOL "" FORCE)

set(SARC_ARCHIVER_BUILD_TEST TRUE CACHE BOOL "" FORCE)

include_directories("ext/zstd/lib")
add_library(zstd STATIC
	ext/zstd/lib/compress/hist.c
	ext/zstd/lib/compress/zstdmt_compress.c
	ext/zstd/lib/compress/zstd_compress_literals.c
	ext/zstd/lib/compress/zstd_compress_sequences.c
	ext/zstd/lib/compress/zstd_compress_superblock.c
	ext/zstd/lib/compress/zstd_opt.c
	ext/zstd/lib/compress/zstd_ldm.c
	ext/zstd/lib/compress/zstd_lazy.c
	ext/zstd/lib/compress/zstd_double_fast.c
	ext/zstd/lib/compress/fse_compress.c
	ext/zstd/lib/compress/huf_compress.c
	ext/zstd/lib/compress/zstd_fast.c
	ext/zstd/lib/compress/zstd_compress.c
	ext/zstd/lib/decompress/zstd_ddict.c
	ext/zstd/lib/decompress/zstd_decompress_block.c
	ext/zstd/lib/decompress/huf_decompress.c
	ext/zstd/lib/decompress/zstd_decompress.c
	ext/zstd/lib/dictBuilder/divsufsort.c
	ext/zstd/lib/dictBuilder/cover.c
	ext/zstd/lib/dictBuilder/fastcover.c
	ext/zstd/lib/dictBuilder/zdict.c
	ext/zstd/lib/common/zstd_common.c
	ext/zstd/lib/common/error_private.c
	ext/zstd/lib/common/xxhash.c
	ext/zstd/lib/common/fse_decompress.c
	ext/zstd/lib/common/debug.c
	ext/zstd/lib/common/threading.c
	ext/zstd/lib/common/pool.c
	ext/zstd/lib/common/entropy_common.c
)

# I can't be bothered to make sure the x86 assembly file is removed from the
# source list on ARM64 builds
target_compile_definitions(zstd PRIVATE ZSTD_DISABLE_ASM)

add_subdirectory(src)
add_subdirectory(ext/physfs)

if (EXISTS "data/")
	file(COPY "data/" DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/src)
endif()

