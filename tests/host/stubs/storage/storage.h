/* Host stub of the Flipper storage API backed by stdio. */
#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
typedef struct Storage Storage;
typedef struct { FILE* f; } File;
typedef enum { FSAM_READ = 1, FSAM_WRITE = 2 } FS_AccessMode;
typedef enum { FSOM_OPEN_EXISTING = 1, FSOM_CREATE_ALWAYS = 4 } FS_OpenMode;
#define EXT_PATH(p) "/tmp/ext/" p
static inline File* storage_file_alloc(Storage* s) { (void)s; return calloc(1, sizeof(File)); }
static inline bool storage_file_open(File* f, const char* p, FS_AccessMode a, FS_OpenMode m) {
    (void)m; f->f = fopen(p, a == FSAM_READ ? "rb" : "wb"); return f->f != NULL; }
static inline size_t storage_file_read(File* f, void* b, size_t n) { return f->f ? fread(b, 1, n, f->f) : 0; }
static inline size_t storage_file_write(File* f, const void* b, size_t n) { return f->f ? fwrite(b, 1, n, f->f) : 0; }
static inline bool storage_file_seek(File* f, uint32_t off, bool start) { (void)start; return f->f && fseek(f->f, off, SEEK_SET) == 0; }
static inline bool storage_file_close(File* f) { if(f->f) fclose(f->f); f->f = NULL; return true; }
static inline void storage_file_free(File* f) { free(f); }
