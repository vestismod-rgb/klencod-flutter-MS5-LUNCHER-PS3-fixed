#ifndef LAUNCH_H
#define LAUNCH_H
#include "scan.h"

typedef enum { LR_OK = 0, LR_NO_WEBMAN, LR_FAIL } LaunchResult;

/* Starts the entry. On the console this normally does not return (the process exits
   so the game can start). Returns LR_NO_WEBMAN / LR_FAIL when it could not be started. */
LaunchResult launch_entry(const Entry *e);

/* exposed for tests: builds the webMAN request path for an entry ("" if none). */
int launch_webman_path(const Entry *e, char *out, int n, int *is_query);
#endif
