#include <stdio.h>
#include <string.h>
#include "../source/launch.h"
int main(void)
{
    char o[400]; int q, fails = 0;
    Entry e; memset(&e, 0, sizeof e);
    e.src = SRC_ISO; strcpy(e.path, "/dev_hdd0/PS3ISO/Dust & Dunes (v1.02) [EU].iso");
    launch_webman_path(&e, o, sizeof o, &q);
    if (strcmp(o, "/dev_hdd0/PS3ISO/Dust%20%26%20Dunes%20%28v1.02%29%20%5BEU%5D.iso") || q) { printf("FAIL iso: %s\n", o); fails++; }
    e.src = SRC_PKG; strcpy(e.id, "NPUB30011");
    launch_webman_path(&e, o, sizeof o, &q);
    if (strcmp(o, "?NPUB30011") || !q) { printf("FAIL pkg: %s\n", o); fails++; }
    e.src = SRC_MM; strcpy(e.path, "/dev_usb000/GAMES/USBGAME");
    launch_webman_path(&e, o, sizeof o, &q);
    if (strcmp(o, "/dev_usb000/GAMES/USBGAME")) { printf("FAIL mm: %s\n", o); fails++; }
    printf(fails ? "launch tests FAILED\n" : "launch tests passed\n");
    return fails;
}
