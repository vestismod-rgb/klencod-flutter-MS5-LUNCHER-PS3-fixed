#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "launch.h"

#ifdef __PPU__
#include <net/net.h>
#include <netinet/in.h>
#include <sys/process.h>
#include <sys/socket.h>
#endif

/* percent-encodes everything except unreserved characters and '/' */
static int urlenc(char *out, int n, const char *in)
{
    static const char hex[] = "0123456789ABCDEF";
    int o = 0;
    for (; *in; in++) {
        unsigned char c = (unsigned char)*in;
        int safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                   c == '/' || c == '.' || c == '_' || c == '-' || c == '~';
        if (safe) { if (o + 1 >= n) return 0; out[o++] = (char)c; }
        else { if (o + 3 >= n) return 0; out[o++] = '%'; out[o++] = hex[c >> 4]; out[o++] = hex[c & 15]; }
    }
    out[o] = 0;
    return 1;
}

/* Returns the part that follows "/play.ps3" or "/mount_ps3".
   Folders and ISOs: "/dev_hdd0/GAMES/NAME" (a path). Installed PSN packages: "?TITLEID" (a query). */
int launch_webman_path(const Entry *e, char *out, int n, int *is_query)
{
    out[0] = 0;
    *is_query = 0;
    if (e->src == SRC_MM || e->src == SRC_ISO) return urlenc(out, n, e->path);
    if (e->src == SRC_PKG && e->id[0]) {
        *is_query = 1;
        snprintf(out, n, "?%s", e->id);
        return 1;
    }
    return 0;
}

#ifdef __PPU__
static int g_net = 0;

static int http_get(const char *request_path, int read_reply)
{
    if (!g_net) { if (netInitialize() < 0) return -1; g_net = 1; }
    int s = netSocket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = 80;               /* PPU is big-endian: host order == network order */
    sa.sin_addr.s_addr = 0x7F000001; /* 127.0.0.1 */
    if (netConnect(s, (struct sockaddr *)&sa, sizeof sa) < 0) { netClose(s); return -1; }
    char req[900];
    int len = snprintf(req, sizeof req, "GET %s HTTP/1.0\r\nHost: localhost\r\nConnection: close\r\n\r\n", request_path);
    if (netSend(s, req, len, 0) < 0) { netClose(s); return -1; }
    if (read_reply) {
        char buf[256];
        while (netRecv(s, buf, sizeof buf, 0) > 0) { }
    }
    netClose(s);
    return 0;
}

LaunchResult launch_entry(const Entry *e)
{
    if (e->src == SRC_APP) {
        char eboot[300];
        snprintf(eboot, sizeof eboot, "%s/USRDIR/EBOOT.BIN", e->path);
        const char *argv[2] = { eboot, NULL };
        sysProcessExitSpawn2(eboot, argv, NULL, NULL, 0, 1001, SYS_PROCESS_SPAWN_STACK_SIZE_1M);
        return LR_FAIL; /* only reached on error */
    }
    char tail[600], req[700];
    int is_query;
    if (!launch_webman_path(e, tail, sizeof tail, &is_query)) return LR_FAIL;
    if (!is_query) {
        snprintf(req, sizeof req, "/mount_ps3%s", tail);
        if (http_get(req, 1) < 0) return LR_NO_WEBMAN;
        usleep(800 * 1000);
        snprintf(req, sizeof req, "/play.ps3%s", tail);
        http_get(req, 0);
    } else {
        snprintf(req, sizeof req, "/play.ps3%s", tail);
        if (http_get(req, 0) < 0) return LR_NO_WEBMAN;
    }
    /* leave the foreground so the game (or the mounted disc icon in the XMB) can take over */
    usleep(1200 * 1000);
    sysProcessExit(0);
    return LR_OK;
}
#else
LaunchResult launch_entry(const Entry *e)
{
    char tail[600];
    int q;
    if (e->src == SRC_APP) { printf("[host] spawn %s/USRDIR/EBOOT.BIN\n", e->path); return LR_FAIL; }
    if (!launch_webman_path(e, tail, sizeof tail, &q)) return LR_FAIL;
    printf("[host] GET /mount_ps3%s then /play.ps3%s\n", q ? "" : tail, tail);
    return LR_NO_WEBMAN;
}
#endif
