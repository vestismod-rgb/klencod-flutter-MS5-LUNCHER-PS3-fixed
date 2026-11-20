#ifndef UI_H
#define UI_H
#include "gfx.h"
#include "scan.h"
#include "pages.h"

typedef struct { int left, right, up, down, circle, cross, square, tri, l1, r1; } Pad;
typedef enum { UA_NONE = 0, UA_LAUNCH, UA_EXIT } UiAction;
typedef enum { M_SCANNING, M_LOGIN, M_BROWSE, M_PLAYCONFIRM, M_EXITCONFIRM, M_LAUNCHING, M_MSG, M_DELCONFIRM, M_POPUP } Mode;

/* Tabs: 0 Games (multiMAN + ISO + PKG merged), 1 Apps, then pages. */
#define TAB_GAMES 0
#define TAB_APPS  1
#define TAB_MEDIA 2
#define TAB_SET   3
#define TAB_STORE 4
#define TAB_FILES 5
#define NTABS     6
#define NLISTTABS 2
#define MAXMV 128

enum { POP_MOVE, POP_FILES, POP_STORE };

typedef struct Ui {
    List *lists;                 /* SRC_COUNT lists, owned by the caller (only SRC_MM and SRC_APP are shown) */
    Mode mode;
    int tab, sel[NLISTTABS];
    float selPos, tabX, tabW, phase;
    Surf scn[2];
    int cur, fade;
    uint32_t hash[2], want, settle;
    int frame, heroT, dlgT, launch_t, msg;
    Pad prev;
    int rep[4];
    const Entry *launch_entry;
    Settings set;
    int spage, scat, srow;       /* settings: page (0 categories, 1 user, 2 general, 3 theme), category cursor, row cursor */
    int media_sel, login_sel;
    Files files; Store store;
    struct { int kind, n, ids[4], cur; } pop;
    char data_dir[260];
    char mv[MAXMV][200]; int nmv;    /* items the user moved between Games and Apps */
} Ui;

int  ui_init(Ui *u, List *lists, const char *data_dir);
void ui_free(Ui *u);
void ui_reset(Ui *u);                       /* after (re)filling the lists: applies moves, resets cursor */
void ui_set_mode(Ui *u, Mode m);
void ui_show_message(Ui *u, int label_id);
UiAction ui_update(Ui *u, const Pad *pad);
void ui_draw(Ui *u, Surf *fb);

/* helpers shared with pages.c */
int  tr_w(const Ui *u, int id);
void tr_draw(Surf *fb, const Ui *u, int id, int x, int cy, uint32_t c, int a);
void ui_panel(Surf *fb, const Ui *u, int x, int y, int w, int h, int r, int selected);
void ui_apply_theme(Ui *u);
#endif
