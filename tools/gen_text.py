#!/usr/bin/env python3
"""Generates source/textdata.c and source/textdata.h.

Pre-renders (1) an ASCII glyph atlas at three sizes and (2) the static Arabic UI
labels (already shaped, right-to-left) as 8-bit alpha bitmaps, so the PS3 app
needs no font library at run time.

Usage: python3 tools/gen_text.py [--font REGULAR.ttf] [--bold BOLD.ttf]
Needs Pillow built with raqm (for Arabic shaping).
"""
import argparse, os, sys
from PIL import Image, ImageDraw, ImageFont, features

ap = argparse.ArgumentParser()
ap.add_argument("--font", default=os.path.join(os.path.dirname(__file__), "..", "fonts", "Tajawal-Medium.ttf"))
ap.add_argument("--bold", default=os.path.join(os.path.dirname(__file__), "..", "fonts", "Tajawal-Bold.ttf"))
ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "..", "source"))
args = ap.parse_args()
for _f in (args.font, args.bold):
    if not os.path.exists(_f):
        sys.exit("Missing font %s - run tools/fetch_fonts.sh first (or pass --font/--bold)" % _f)
if not features.check("raqm"):
    sys.exit("Pillow needs raqm support for Arabic shaping")

LATIN = [("small", args.font, 20), ("reg", args.font, 24), ("med", args.bold, 26), ("title", args.bold, 36), ("big", args.bold, 56)]

# (key, English, Arabic, pixel size, bold)
# Arabic is pre-shaped into bitmaps; English is drawn from the Latin atlas at run time.
LABELS = [
 ("TAB_GAMES","Games","الألعاب",26,1), ("TAB_APPS","Apps","التطبيقات",26,1), ("TAB_MEDIA","Media","وسائط",26,1),
 ("TAB_SET","Settings","الإعدادات",26,1), ("TAB_STORE","Store","المتجر",26,1), ("TAB_FILES","Files","الملفات",26,1),
 ("H_NAV","Navigate","تنقّل",20,0), ("H_MENU","Menu","القائمة",20,0), ("H_BACK","Back","رجوع",20,0),
 ("H_OK","Confirm","تأكيد",20,0), ("H_SEL","Select","تحديد",20,0),
 ("BTN_PLAY","Play","العب الآن",26,1), ("BTN_OPEN","Open","فتح",26,1),
 ("LBL_ITEMS","items","عنصر",20,0), ("B_MM","multiMAN","ملتي مان",20,1), ("B_APP","App","تطبيق",20,1),
 ("NO_GAMES","No games yet","لا توجد ألعاب بعد",36,1), ("NO_APPS","No apps yet","لا توجد تطبيقات بعد",36,1),
 ("EMPTY_HINT","Put games in GAMES or PS3ISO on the HDD or USB","ضع الألعاب في GAMES أو PS3ISO على القرص أو USB",20,0),
 ("SCANNING","Scanning for games...","جارٍ البحث عن الألعاب…",26,0),
 ("LAUNCHING","Launching...","جارٍ التشغيل…",36,1),
 ("NOWM","This item needs webMAN MOD to launch","يتطلب تشغيل هذا النوع وجود webMAN MOD",24,0),
 ("FAIL","Could not launch","تعذّر التشغيل",24,0),
 ("DLG_PLAY","Launch this item?","هل تريد التشغيل؟",36,1), ("DLG_EXIT","Exit MS5 LUNCHER?","الخروج من البرنامج؟",36,1),
 ("MV_APPS","Add to Apps","إضافة إلى التطبيقات",24,0), ("MV_GAMES","Add to Games","إضافة إلى الألعاب",24,0),
 ("C_USER","User","المستخدم",26,1), ("C_USER_D","Name & users","الاسم والمستخدمون",20,0),
 ("C_GEN","General","عام",26,1), ("C_GEN_D","Language & options","اللغة والخيارات",20,0),
 ("C_THEME","Theme","المظهر",26,1), ("C_THEME_D","Colors, background & opacity","الألوان والخلفية والشفافية",20,0),
 ("T_USER","User","المستخدم",36,1), ("T_GEN","General","عام",36,1), ("T_THEME","Theme","المظهر",36,1),
 ("T_SET","Settings","الإعدادات",36,1), ("T_STORE","Store","المتجر",36,1), ("T_FILES","Files","الملفات",36,1), ("T_MEDIA","Media","وسائط",36,1),
 ("R_SWITCH","Switch user","تبديل المستخدم",24,0), ("R_ADD","Add user","إضافة مستخدم",24,0), ("R_DEL","Delete user","حذف المستخدم",24,0),
 ("BTN_SWITCH","Switch user","تبديل",24,1), ("BTN_ADD","Add user","إضافة",24,1), ("BTN_DEL","Delete user","حذف",24,1), ("BTN_RESET","Reset theme","إعادة الضبط",24,1),
 ("R_LANG","Language","اللغة",24,0), ("R_SPEED","Animation speed","سرعة الحركة",24,0),
 ("R_MOTION","Background motion","حركة الخلفية",24,0), ("R_MOTION_D","Waves & particles","أمواج وجسيمات",20,0),
 ("R_24H","24-hour clock","نظام 24 ساعة",24,0),
 ("R_ASKSTART","Ask who is using at startup","السؤال عن المستخدم عند البدء",24,0), ("R_ASKSTART_D","If off, the last user signs in automatically","عند الإيقاف يدخل آخر مستخدم تلقائياً",20,0),
 ("R_ASKPLAY","Confirm before launching","تأكيد قبل التشغيل",24,0),
 ("R_BG","Background color","لون الخلفية",24,0), ("R_PANEL","Panel opacity","شفافية اللوحات",24,0), ("R_DIM","Background dimming","تعتيم الخلفية",24,0),
 ("R_WAVES","Waves","الأمواج",24,0), ("R_ACCENT","Accent color","لون التمييز",24,0), ("R_ICON","Icon size","حجم الأيقونات",24,0),
 ("R_RAD","Icon roundness","استدارة الأيقونات",24,0), ("R_RESET","Reset theme","إعادة ضبط المظهر",24,0),
 ("WHO","Who is using this?","من يستخدم؟",40,1), ("ADDUSER","Add user","إضافة مستخدم",24,0), ("PLAYER","Player","لاعب",24,0),
 ("F_EMPTY","This folder is empty","المجلد فارغ",26,0),
 ("F_DEL_Q","Delete permanently?","حذف نهائياً؟",36,1), ("F_DEL_FAIL","Could not delete (folder not empty or read-only)","تعذّر الحذف (المجلد غير فارغ أو للقراءة فقط)",24,0),
 ("F_DELETE","Delete","حذف",24,0), ("F_SELALL","Select all","تحديد الكل",24,0), ("F_CLR","Clear selection","إلغاء التحديد",24,0),
 ("STORE_ALL","All","الكل",20,1), ("STORE_HINT","for menu","للقائمة",20,0), ("NO_RESULTS","No results","لا توجد نتائج",26,0),
 ("STORE_EMPTY","No catalog. Put store.txt in USRDIR","لا يوجد كتالوج. ضع الملف store.txt في مجلد USRDIR",26,0),
 ("STORE_NA","Installing from the store is not available yet","التثبيت من المتجر غير متاح بعد",26,0),
 ("M_VIDEO","Video","فيديو",26,1), ("M_MUSIC","Music","موسيقى",26,1), ("M_PHOTOS","Photos","الصور",26,1), ("M_BROWSER","Browser","المتصفح",26,1),
 ("M_NA","The browser is not available in this launcher","المتصفح غير متوفر في هذا البرنامج",26,0),
 ("LANG_AR","Arabic","العربية",22,1),
]

def style_of(size, bold):
    if bold: return 2 if size <= 28 else (3 if size <= 40 else 4)
    return 0 if size <= 21 else 1

def carr(name, data):
    out = ["static const unsigned char %s[] = {" % name]
    line = []
    for i, b in enumerate(data):
        line.append(str(b))
        if len(line) == 32:
            out.append(",".join(line) + ","); line = []
    if line: out.append(",".join(line) + ",")
    if not data: out.append("0,")
    out.append("};")
    return "\n".join(out)

c = ['/* GENERATED by tools/gen_text.py - do not edit. */', '#include "textdata.h"', '']
for name, path, size in LATIN:
    f = ImageFont.truetype(path, size)
    asc, desc = f.getmetrics()
    pad = 4
    px = bytearray(); glyphs = []
    for code in range(32, 127):
        ch = chr(code)
        adv = int(round(f.getlength(ch)))
        im = Image.new("L", (size * 2 + pad * 2, (asc + desc) * 2 + pad * 2), 0)
        d = ImageDraw.Draw(im)
        base_x, base_y = pad, asc + pad
        d.text((base_x, base_y), ch, font=f, fill=255, anchor="ls")
        bb = im.getbbox()
        if not bb:
            glyphs.append((0, 0, adv, 0, 0, len(px))); continue
        crop = im.crop(bb)
        glyphs.append((crop.width, crop.height, adv, bb[0] - base_x, bb[1] - base_y, len(px)))
        px.extend(crop.tobytes())
    c.append(carr("px_" + name, px))
    c.append("static const Glyph gl_%s[95] = {" % name)
    for g in glyphs:
        c.append("{%d,%d,%d,%d,%d,%d}," % g)
    c.append("};")
    c.append("const Font font_%s = {%d,%d,gl_%s,px_%s};" % (name, asc, asc + desc, name, name))
    c.append("")

enum = ["/* GENERATED by tools/gen_text.py - do not edit. */", "#ifndef TEXTDATA_H", "#define TEXTDATA_H",
        "typedef struct { int w, h, adv, bx, by, off; } Glyph;",
        "typedef struct { int ascent, line; const Glyph *g; const unsigned char *px; } Font;",
        "typedef struct { int w, h; const unsigned char *px; } Label;",
        "extern const Font font_small, font_reg, font_med, font_title, font_big;", "enum {"]
for i, (key, *_r) in enumerate(LABELS):
    enum.append("  L_%s = %d," % (key, i))
enum += ["  L_COUNT = %d" % len(LABELS), "};",
         "extern const Label labels[L_COUNT];            /* Arabic, pre-shaped bitmaps */",
         "extern const char *const label_en[L_COUNT];    /* English text */",
         "extern const unsigned char label_style[L_COUNT]; /* 0 small, 1 reg, 2 med, 3 title, 4 big */", "#endif", ""]

ldefs = []; en = []; st = []
for key, e, text, size, bold in LABELS:
    f = ImageFont.truetype(args.bold if bold else args.font, size, layout_engine=(ImageFont.Layout.RAQM if hasattr(ImageFont, "Layout") else ImageFont.LAYOUT_RAQM))
    im = Image.new("L", (size * len(text) + 40, size * 3), 0)
    d = ImageDraw.Draw(im)
    d.text((20, size), text, font=f, fill=255, direction="rtl", language="ar", anchor="la")
    bb = im.getbbox()
    crop = im.crop((bb[0] - 2, bb[1] - 2, bb[2] + 2, bb[3] + 2)) if bb else Image.new("L", (1, 1), 0)
    c.append(carr("lb_" + key, crop.tobytes()))
    ldefs.append("{%d,%d,lb_%s}," % (crop.width, crop.height, key))
    en.append('"%s",' % e.replace('"', '\\"'))
    st.append("%d," % style_of(size, bold))
c.append("const Label labels[L_COUNT] = {"); c += ldefs; c.append("};")
c.append("const char *const label_en[L_COUNT] = {"); c += en; c.append("};")
c.append("const unsigned char label_style[L_COUNT] = {"); c += st; c.append("};")
out = os.path.abspath(args.out)
open(os.path.join(out, "textdata.c"), "w").write("\n".join(c) + "\n")
open(os.path.join(out, "textdata.h"), "w").write("\n".join(enum))
print("wrote", out)
