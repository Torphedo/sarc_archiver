#pragma once
// Structures to parse SARC (SEAD Archive) files from Nintendo games.

#include <stdint.h>

typedef enum {
  // Header
  SARC_MAGIC = 0x43524153, // 'SARC'
  SARC_HEADER_SIZE = 0x14,
  SARC_LITTLE_ENDIAN = 0xFEFF,
  SARC_BIG_ENDIAN = 0xFFFE,
  SARC_VERSION = 0x0100,

  // SFAT 
  SFAT_MAGIC = 0x54414653, // 'SFAT'
  SFAT_HEADER_SIZE = 0xC,
  SFAT_HASH_KEY = (uint32_t) 0x00000065,

  SFNT_MAGIC = 0x544E4653, // 'SFNT'
  SFNT_HEADER_SIZE = 0x8
}sarc_constants;

// Archive header
typedef struct {
  uint32_t magic; // 'SARC'
  uint16_t header_size; // 0x14
  uint16_t byte_order_mark;
  uint32_t archive_size;
  uint32_t data_offset; // Position where the file data starts.
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
  uint16_t filename_offset;
  uint16_t enable_offset; // This is 1 when the above field is valid
  uint32_t file_start_offset;
  uint32_t file_end_offset;
}sarc_sfat_node;


// SARC File Name Table (SFNT) structures

typedef struct {
  uint32_t magic; // 'SFNT'
  uint16_t header_size; // 0x8
  uint16_t reserved;
}sarc_sfnt_header;

// Hash code taken from here (almost verbatim):
// https://mk8.tockdom.com/wiki/SARC_(File_Format)#File_Name_Hash
uint32_t sarc_filename_hash(char* name, uint32_t length, uint32_t key) {
  uint32_t result = 0;
  for (uint32_t i = 0; i < length; i++) {
    result = name[i] + (result * key);
  }
  return result;
}

