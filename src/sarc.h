#pragma once
// Structures to parse SARC (SEAD Archive) files from Nintendo games.

#include <stdint.h>

// Archive header
typedef struct {
    uint32_t magic; // 'SARC'
    uint16_t header_size; // 0x14
    uint16_t byte_order_mark;
    uint32_t archive_size;
    uint32_t data_offset; // Position where the file data start.
    uint16_t version;
    uint16_t reserved;
}sarc_header;


// SARC File Allocation Table (SFAT) structures

typedef struct {
    uint32_t magic; // 'SFAT'
    uint16_t header_size; // 0xC
    uint16_t node_count;
    uint32_t hash_key; // 0x65
}sarc_sfat_header;

// Offsets in this structure are relative to header.data_offset
typedef struct {
    uint32_t filename_hash;
    uint32_t file_attributes;
    uint32_t file_start_offset;
    uint32_t file_end_offset;
}sarc_sfat_node;


// SARC File Name Table (SFNT) structures

typedef struct {
    uint32_t magic; // 'SFNT'
    uint16_t header_size; // 0x8
    uint16_t reserved;
}sarc_sfnt_header;


