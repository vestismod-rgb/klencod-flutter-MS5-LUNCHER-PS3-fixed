#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../source/scan.h"
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
int main(int argc, char **argv)
{
    scan_set_root(argv[1]);
    List l[SRC_COUNT] = {{0}};
    scan_all(l);
    const char *names[] = {"MM", "ISO", "PKG", "APP"};
    for (int s = 0; s < SRC_COUNT; s++) {
        printf("== %s (%d)\n", names[s], l[s].n);
        for (int i = 0; i < l[s].n; i++) printf("  %-52s %-10s %s\n", l[s].items[i].title, l[s].items[i].id, l[s].items[i].path);
    }
    CHECK(l[SRC_MM].n == 7);   /* 6 hdd + 1 usb, NOT_A_GAME skipped */
    CHECK(l[SRC_ISO].n == 3);
    CHECK(l[SRC_PKG].n == 2);
    CHECK(l[SRC_APP].n == 3);
    CHECK(!strcmp(l[SRC_MM].items[0].title, "Dust & Dunes: Extreme Edition of the Long Title Test"));
    int found = 0, broken = 0;
    for (int i = 0; i < l[SRC_ISO].n; i++) {
        if (!strcmp(l[SRC_ISO].items[i].title, "Deep Reef ISO") && !strcmp(l[SRC_ISO].items[i].id, "BLES20001")) found = 1;
        if (!strcmp(l[SRC_ISO].items[i].title, "Broken_Title")) broken = 1;
    }
    CHECK(found); CHECK(broken);
    scan_free(l);
    printf(fails ? "TESTS FAILED\n" : "scan tests passed\n");
    return fails != 0;
}
