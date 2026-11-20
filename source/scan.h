#ifndef SCAN_H
#define SCAN_H
#include <stdint.h>

typedef enum { SRC_MM = 0, SRC_ISO, SRC_PKG, SRC_APP, SRC_COUNT } Source;

typedef struct {
    char title[96];
    char id[16];        /* TITLE_ID from PARAM.SFO, may be empty */
    char path[256];     /* folder (GAMES / game dir) or .iso file, without the root prefix */
    uint32_t hash;      /* seed for generated artwork */
    Source src;
} Entry;

typedef struct { Entry *items; int n, cap; } List;

/* Prefix used for every filesystem access. "" on the console; a temp dir in host tests. */
void scan_set_root(const char *root);
const char *scan_root(void);
/* Fills lists[SRC_COUNT]; frees previous contents. */
void scan_all(List lists[SRC_COUNT]);
void scan_merge_games(List lists[SRC_COUNT]); /* call after scan_all: ISO/PKG -> games list */
void scan_free(List lists[SRC_COUNT]);

/* exposed for tests */
int sfo_parse(const uint8_t *buf, int len, char *title, int tn, char *id, int in);
int iso_read_sfo(const char *file, char *title, int tn, char *id, int in);
#endif
