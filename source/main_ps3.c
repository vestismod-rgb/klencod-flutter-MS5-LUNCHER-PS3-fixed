/* PS3 entry point: video (RSX framebuffers), pad input, main loop. */
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ppu-types.h>
#include <io/pad.h>
#include <rsx/rsx.h>
#include <sysutil/sysutil.h>
#include <sys/process.h>
#include <sysutil/video.h>

#include "gfx.h"
#include "launch.h"
#include "scan.h"
#include "ui.h"

#define CB_SIZE   0x100000
#define HOST_SIZE (32 * 1024 * 1024)

static gcmContextData *g_ctx;
static videoResolution g_res;
static u32 *g_fbptr[2];
static u32 g_fboff[2];
static int g_first = 1, g_exit = 0;

static void sysutil_cb(u64 status, u64 param, void *usrdata)
{
    (void)param; (void)usrdata;
    if (status == SYSUTIL_EXIT_GAME) g_exit = 1;
}

static void wait_flip(void)
{
    while (gcmGetFlipStatus() != 0) usleep(200);
    gcmResetFlipStatus();
}

static void flip(int idx)
{
    if (g_first) { gcmResetFlipStatus(); g_first = 0; }
    else wait_flip();
    if (gcmSetFlip(g_ctx, idx) == 0) {
        rsxFlushBuffer(g_ctx);
        gcmSetWaitFlip(g_ctx);
    }
}

static int init_video(void)
{
    void *host = memalign(1024 * 1024, HOST_SIZE);
    if (!host) return 0;
    rsxInit(&g_ctx, CB_SIZE, HOST_SIZE, host);

    videoState state;
    videoGetState(0, 0, &state);
    int res_id = state.displayMode.resolution;
    if (videoGetResolutionAvailability(0, VIDEO_RESOLUTION_720, VIDEO_ASPECT_AUTO, 0)) res_id = VIDEO_RESOLUTION_720;
    videoGetResolution(res_id, &g_res);

    videoConfiguration vc;
    memset(&vc, 0, sizeof vc);
    vc.resolution = res_id;
    vc.format = VIDEO_BUFFER_FORMAT_XRGB;
    vc.pitch = g_res.width * sizeof(u32);
    vc.aspect = VIDEO_ASPECT_AUTO;
    videoConfigure(0, &vc, NULL, 0);
    videoGetState(0, 0, &state);

    gcmSetFlipMode(GCM_FLIP_VSYNC);
    for (int i = 0; i < 2; i++) {
        g_fbptr[i] = (u32 *)rsxMemalign(64, g_res.width * g_res.height * sizeof(u32));
        if (!g_fbptr[i]) return 0;
        rsxAddressToOffset(g_fbptr[i], &g_fboff[i]);
        gcmSetDisplayBuffer(i, g_fboff[i], g_res.width * sizeof(u32), g_res.width, g_res.height);
        memset(g_fbptr[i], 0, g_res.width * g_res.height * sizeof(u32));
    }
    gcmResetFlipStatus();
    return 1;
}

static void read_pad(Pad *p)
{
    padInfo info;
    padData data;
    memset(p, 0, sizeof *p);
    ioPadGetInfo(&info);
    for (int i = 0; i < MAX_PADS; i++) {
        if (!info.status[i]) continue;
        ioPadGetData(i, &data);
        p->left   |= data.BTN_LEFT;
        p->right  |= data.BTN_RIGHT;
        p->up     |= data.BTN_UP;
        p->down   |= data.BTN_DOWN;
        p->circle |= data.BTN_CIRCLE;   /* confirm */
        p->cross  |= data.BTN_CROSS;    /* back */
        p->square |= data.BTN_SQUARE;   /* select (Files) */
        p->tri    |= data.BTN_TRIANGLE; /* menu */
        p->l1     |= data.BTN_L1;
        p->r1     |= data.BTN_R1;
    }
}

/* copy the software frame into the RSX buffer (rows may differ if the TV is not 720p) */
static void present(const Surf *fb, int idx)
{
    int rw = (int)g_res.width, rh = (int)g_res.height;
    int w = rw < fb->w ? rw : fb->w;
    int h = rh < fb->h ? rh : fb->h;
    if (rw == fb->w && rh == fb->h) {
        memcpy(g_fbptr[idx], fb->px, (size_t)fb->w * fb->h * 4);
    } else {
        for (int y = 0; y < h; y++) memcpy(g_fbptr[idx] + y * g_res.width, fb->px + y * fb->w, (size_t)w * 4);
    }
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sysutil_cb, NULL);
    if (!init_video()) return 1;
    ioPadInit(7);

    Surf fb;
    fb.w = SW; fb.h = SH;
    fb.px = (uint32_t *)malloc((size_t)SW * SH * 4);
    List lists[SRC_COUNT];
    memset(lists, 0, sizeof lists);
    static Ui ui;
    if (!fb.px || !ui_init(&ui, lists, "/dev_hdd0/game/MS5LNCHR1/USRDIR")) return 1;

    int cur = 0;
    /* two "scanning" frames, then the (blocking) scan */
    for (int i = 0; i < 2; i++) {
        sysUtilCheckCallback();
        ui_draw(&ui, &fb); present(&fb, cur); flip(cur); cur ^= 1;
    }
    scan_all(lists);
    scan_merge_games(lists);
    ui_reset(&ui);
    ui_set_mode(&ui, ui.set.askstart ? M_LOGIN : M_BROWSE);

    while (!g_exit) {
        sysUtilCheckCallback();
        Pad pad;
        read_pad(&pad);
        UiAction a = ui_update(&ui, &pad);
        if (a == UA_EXIT) break;
        if (a == UA_LAUNCH && ui.launch_entry) {
            LaunchResult r = launch_entry(ui.launch_entry); /* normally never returns */
            ui_show_message(&ui, r == LR_NO_WEBMAN ? L_NOWM : L_FAIL);
        }
        ui_draw(&ui, &fb);
        present(&fb, cur);
        flip(cur);
        cur ^= 1;
    }

    ioPadEnd();
    sysProcessExit(0);
    return 0;
}
