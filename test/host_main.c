#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../source/ui.h"
#include "../source/scan.h"

static void shot(Ui *u, Surf *fb, const char *name)
{
    ui_draw(u, fb);
    char path[200]; snprintf(path, sizeof path, "%s/%s.ppm", getenv("OUT") ? getenv("OUT") : "/tmp/frames", name);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", fb->w, fb->h);
    for (int i = 0; i < SW * SH; i++) { unsigned char c[3] = { fb->px[i] >> 16, fb->px[i] >> 8, fb->px[i] }; fwrite(c, 1, 3, f); }
    fclose(f);
}
static UiAction step(Ui *u, Pad p, int n)
{
    UiAction a = UA_NONE, last = UA_NONE;
    for (int i = 0; i < n; i++) { a = ui_update(u, &p); if (a) last = a; memset(&p, 0, sizeof p); }
    return last;
}
static Pad P(const char *k)
{
    Pad p = {0};
    if (!strcmp(k, "down")) p.down = 1; else if (!strcmp(k, "up")) p.up = 1;
    else if (!strcmp(k, "right")) p.right = 1; else if (!strcmp(k, "left")) p.left = 1;
    else if (!strcmp(k, "r1")) p.r1 = 1; else if (!strcmp(k, "l1")) p.l1 = 1;
    else if (!strcmp(k, "o")) p.circle = 1; else if (!strcmp(k, "x")) p.cross = 1;
    else if (!strcmp(k, "sq")) p.square = 1; else if (!strcmp(k, "tri")) p.tri = 1;
    return p;
}
#define TAP(k) do { step(&u, P(k), 1); step(&u, P("none"), 2); } while (0)
#define SETTLE(n) step(&u, P("none"), n)

int main(int argc, char **argv)
{
    (void)argc;
    scan_set_root(argv[1]);
    List lists[SRC_COUNT] = {{0}};
    Ui u; Surf fb = { malloc((size_t)SW * SH * 4), SW, SH };
    if (system("rm -rf /tmp/ms5data && mkdir -p /tmp/ms5data && cp test/store.txt /tmp/ms5data/store.txt")) return 1;
    if (!ui_init(&u, lists, "/tmp/ms5data")) return 1;
    shot(&u, &fb, "00_scanning");
    scan_all(lists); scan_merge_games(lists);
    ui_set_mode(&u, M_LOGIN); ui_reset(&u); ui_set_mode(&u, M_LOGIN);
    SETTLE(30); shot(&u, &fb, "01_login");
    TAP("o"); SETTLE(30);                                  /* pick Player 1 */
    shot(&u, &fb, "02_games");
    printf("games=%d apps=%d iso+pkg lists empty=%d\n", u.lists[SRC_MM].n, u.lists[SRC_APP].n, u.lists[SRC_ISO].n + u.lists[SRC_PKG].n == 0);
    /* move a game to Apps with the Triangle menu */
    int g0 = u.lists[SRC_MM].n;
    TAP("tri"); SETTLE(12); shot(&u, &fb, "03_menu_move");
    TAP("o"); SETTLE(10);
    printf("after move: games=%d (was %d) apps=%d\n", u.lists[SRC_MM].n, g0, u.lists[SRC_APP].n);
    TAP("r1"); SETTLE(30); shot(&u, &fb, "04_apps");
    TAP("r1"); SETTLE(30); shot(&u, &fb, "05_media");
    TAP("r1"); SETTLE(20); shot(&u, &fb, "06_settings_cats");
    TAP("o"); SETTLE(10); shot(&u, &fb, "07_set_user");
    TAP("x"); TAP("right"); TAP("o"); SETTLE(10); shot(&u, &fb, "08_set_general");
    TAP("down"); TAP("down"); TAP("o");                    /* waves off? (row 2 = motion) */
    TAP("x"); TAP("right"); TAP("o"); SETTLE(10);
    TAP("right"); TAP("right"); TAP("down"); TAP("right"); TAP("right"); TAP("right"); /* bg changes */
    SETTLE(20); shot(&u, &fb, "09_set_theme");
    TAP("x"); TAP("r1"); SETTLE(20); shot(&u, &fb, "10_store");
    TAP("tri"); SETTLE(12); shot(&u, &fb, "11_store_menu");
    TAP("down"); TAP("o"); SETTLE(10);
    TAP("r1"); SETTLE(20); shot(&u, &fb, "12_files_root");
    TAP("o"); SETTLE(2); shot(&u, &fb, "13_files_hdd");
    { char cmd[400]; snprintf(cmd, sizeof cmd, "mkdir -p %s/dev_hdd0/tmp && echo x > %s/dev_hdd0/tmp/del_me.txt && echo y > %s/dev_hdd0/tmp/del_me2.txt", argv[1], argv[1], argv[1]); if (system(cmd)) return 1; }
    files_open(&u.files, "/dev_hdd0/tmp");
    TAP("sq"); TAP("down"); TAP("sq"); SETTLE(5); shot(&u, &fb, "14_files_selected");
    TAP("tri"); SETTLE(12); shot(&u, &fb, "15_files_menu");
    TAP("o"); SETTLE(12); shot(&u, &fb, "16_delete_confirm");
    TAP("o"); SETTLE(12);
    { char p[300]; snprintf(p, sizeof p, "%s/dev_hdd0/tmp/del_me.txt", argv[1]); printf("deleted=%d\n", access(p, F_OK) != 0); }
    { Settings c; settings_load(&c, "/tmp/ms5data/settings.cfg"); printf("saved bg=%d accent=%d lang=%d anim=%d\n", c.bg, c.accent, c.lang, c.anim); }
    { char cmd[300]; snprintf(cmd, sizeof cmd, "wc -l < /tmp/ms5data/moves.cfg"); if (system(cmd)) return 1; }
    u.set.lang = 1; ui_apply_theme(&u); u.tab = 0; SETTLE(40); shot(&u, &fb, "17_games_english");
    u.tab = 5; shot(&u, &fb, "18_files_english");
    u.set.lang = 0;
    clock_t t0 = clock();
    u.tab = 0;
    for (int i = 0; i < 60; i++) { step(&u, P("none"), 1); ui_draw(&u, &fb); }
    printf("host draw: %.2f ms/frame\n", 1000.0 * (clock() - t0) / CLOCKS_PER_SEC / 60);
    ui_free(&u); scan_free(lists); free(fb.px);
    return 0;
}
