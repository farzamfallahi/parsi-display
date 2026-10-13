/*
 * ps_shaper.c - Persian contextual shaping, simplified bidi and UTF-8 helpers.
 * See ps_shaper.h for the public API.
 */
#include "ps_shaper.h"

#define CP_ZWNJ             0x200Cu   /* zero-width non-joiner (half space) */
#define CP_TATWEEL          0x0640u
#define CP_LAM              0x0644u
#define CP_ALEF_MADDA       0x0622u
#define CP_ALEF_HAMZA_ABOVE 0x0623u
#define CP_ALEF_HAMZA_BELOW 0x0625u
#define CP_ALEF             0x0627u

/* Presentation forms: isolated, final, initial, medial (0 = none).
 * A letter with no initial form only joins to the letter before it.
 * Sorted by base code point for binary search. */
typedef struct {
    uint16_t base, iso, fin, ini, med;
} ps_form;

static const ps_form FORMS[] = {
    {0x0621, 0xFE80, 0,      0,      0     }, /* hamza          */
    {0x0622, 0xFE81, 0xFE82, 0,      0     }, /* alef madda     */
    {0x0623, 0xFE83, 0xFE84, 0,      0     },
    {0x0624, 0xFE85, 0xFE86, 0,      0     },
    {0x0625, 0xFE87, 0xFE88, 0,      0     },
    {0x0626, 0xFE89, 0xFE8A, 0xFE8B, 0xFE8C},
    {0x0627, 0xFE8D, 0xFE8E, 0,      0     }, /* alef           */
    {0x0628, 0xFE8F, 0xFE90, 0xFE91, 0xFE92}, /* beh            */
    {0x0629, 0xFE93, 0xFE94, 0,      0     },
    {0x062A, 0xFE95, 0xFE96, 0xFE97, 0xFE98}, /* teh            */
    {0x062B, 0xFE99, 0xFE9A, 0xFE9B, 0xFE9C},
    {0x062C, 0xFE9D, 0xFE9E, 0xFE9F, 0xFEA0},
    {0x062D, 0xFEA1, 0xFEA2, 0xFEA3, 0xFEA4},
    {0x062E, 0xFEA5, 0xFEA6, 0xFEA7, 0xFEA8},
    {0x062F, 0xFEA9, 0xFEAA, 0,      0     }, /* dal            */
    {0x0630, 0xFEAB, 0xFEAC, 0,      0     },
    {0x0631, 0xFEAD, 0xFEAE, 0,      0     }, /* reh            */
    {0x0632, 0xFEAF, 0xFEB0, 0,      0     },
    {0x0633, 0xFEB1, 0xFEB2, 0xFEB3, 0xFEB4}, /* seen           */
    {0x0634, 0xFEB5, 0xFEB6, 0xFEB7, 0xFEB8},
    {0x0635, 0xFEB9, 0xFEBA, 0xFEBB, 0xFEBC},
    {0x0636, 0xFEBD, 0xFEBE, 0xFEBF, 0xFEC0},
    {0x0637, 0xFEC1, 0xFEC2, 0xFEC3, 0xFEC4},
    {0x0638, 0xFEC5, 0xFEC6, 0xFEC7, 0xFEC8},
    {0x0639, 0xFEC9, 0xFECA, 0xFECB, 0xFECC},
    {0x063A, 0xFECD, 0xFECE, 0xFECF, 0xFED0},
    {0x0641, 0xFED1, 0xFED2, 0xFED3, 0xFED4},
    {0x0642, 0xFED5, 0xFED6, 0xFED7, 0xFED8},
    {0x0643, 0xFED9, 0xFEDA, 0xFEDB, 0xFEDC}, /* Arabic kaf     */
    {0x0644, 0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0}, /* lam            */
    {0x0645, 0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4}, /* meem           */
    {0x0646, 0xFEE5, 0xFEE6, 0xFEE7, 0xFEE8}, /* noon           */
    {0x0647, 0xFEE9, 0xFEEA, 0xFEEB, 0xFEEC}, /* heh            */
    {0x0648, 0xFEED, 0xFEEE, 0,      0     }, /* waw            */
    {0x0649, 0xFEEF, 0xFEF0, 0,      0     }, /* alef maksura   */
    {0x064A, 0xFEF1, 0xFEF2, 0xFEF3, 0xFEF4}, /* Arabic yeh     */
    {0x067E, 0xFB56, 0xFB57, 0xFB58, 0xFB59}, /* peh   (Persian) */
    {0x0686, 0xFB7A, 0xFB7B, 0xFB7C, 0xFB7D}, /* tcheh (Persian) */
    {0x0698, 0xFB8A, 0xFB8B, 0,      0     }, /* jeh   (Persian) */
    {0x06A9, 0xFB8E, 0xFB8F, 0xFB90, 0xFB91}, /* keheh (Persian) */
    {0x06AF, 0xFB92, 0xFB93, 0xFB94, 0xFB95}, /* gaf   (Persian) */
    {0x06CC, 0xFBFC, 0xFBFD, 0xFBFE, 0xFBFF}, /* Farsi yeh       */
};
#define FORMS_COUNT (sizeof(FORMS) / sizeof(FORMS[0]))

static const ps_form *find_form(uint32_t cp)
{
    size_t lo = 0, hi = FORMS_COUNT;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (FORMS[mid].base == cp) return &FORMS[mid];
        if (FORMS[mid].base < cp) lo = mid + 1; else hi = mid;
    }
    return NULL;
}

typedef enum { JT_NONE, JT_RIGHT, JT_DUAL, JT_CAUSING, JT_TRANSPARENT } joining_t;

static int is_mark(uint32_t cp)
{
    return (cp >= 0x064B && cp <= 0x065F) || cp == 0x0670;
}

static uint32_t normalise(uint32_t cp, unsigned flags)
{
    if (flags & PS_FLAG_NORMALISE_ARABIC) {
        if (cp == 0x0643) return 0x06A9;                 /* kaf -> keheh   */
        if (cp == 0x064A || cp == 0x0649) return 0x06CC; /* yeh -> Farsi yeh */
    }
    return cp;
}

static joining_t joining_type(uint32_t cp)
{
    const ps_form *f;
    if (is_mark(cp))      return JT_TRANSPARENT;
    if (cp == CP_TATWEEL) return JT_CAUSING;
    f = find_form(cp);
    if (!f || !f->fin)    return JT_NONE;   /* includes hamza and ZWNJ */
    return f->ini ? JT_DUAL : JT_RIGHT;
}

/* Does the nearest non-mark letter before index i connect forward to i? */
static int prev_connects(const uint32_t *in, size_t i, unsigned flags)
{
    while (i > 0) {
        joining_t jt = joining_type(normalise(in[--i], flags));
        if (jt == JT_TRANSPARENT) continue;
        return jt == JT_DUAL || jt == JT_CAUSING;
    }
    return 0;
}

/* Does the nearest non-mark letter after index i accept a connection from i? */
static int next_connects(const uint32_t *in, size_t len, size_t i, unsigned flags)
{
    while (++i < len) {
        joining_t jt = joining_type(normalise(in[i], flags));
        if (jt == JT_TRANSPARENT) continue;
        return jt == JT_DUAL || jt == JT_RIGHT || jt == JT_CAUSING;
    }
    return 0;
}

/* Lam-alef ligature for the given alef variant: isolated / final forms. */
static int lam_alef(uint32_t alef, uint32_t *iso, uint32_t *fin)
{
    switch (alef) {
    case CP_ALEF_MADDA:       *iso = 0xFEF5; *fin = 0xFEF6; return 1;
    case CP_ALEF_HAMZA_ABOVE: *iso = 0xFEF7; *fin = 0xFEF8; return 1;
    case CP_ALEF_HAMZA_BELOW: *iso = 0xFEF9; *fin = 0xFEFA; return 1;
    case CP_ALEF:             *iso = 0xFEFB; *fin = 0xFEFC; return 1;
    default: return 0;
    }
}

ps_status ps_shape(const uint32_t *in, size_t len,
                   uint32_t *out, size_t cap, size_t *out_len, unsigned flags)
{
    size_t i, n = 0;

    if (!in || !out || !out_len) return PS_ERR_ARG;

    for (i = 0; i < len; i++) {
        uint32_t cp = normalise(in[i], flags);
        uint32_t glyph;
        joining_t jt;

        if (cp == CP_ZWNJ) continue;                  /* only breaks joins */
        jt = joining_type(cp);
        if (jt == JT_TRANSPARENT && !(flags & PS_FLAG_KEEP_MARKS)) continue;

        if (cp == CP_LAM && i + 1 < len) {
            uint32_t iso, fin;
            if (lam_alef(normalise(in[i + 1], flags), &iso, &fin)) {
                glyph = prev_connects(in, i, flags) ? fin : iso;
                i++;                                  /* alef consumed */
                if (n >= cap) return PS_ERR_BUFFER;
                out[n++] = glyph;
                continue;
            }
        }

        glyph = cp;
        if (jt == JT_DUAL || jt == JT_RIGHT) {
            const ps_form *f = find_form(cp);
            int joins_prev = prev_connects(in, i, flags);
            int joins_next = (jt == JT_DUAL) && next_connects(in, len, i, flags);
            if (joins_prev && joins_next) glyph = f->med;
            else if (joins_prev)          glyph = f->fin;
            else if (joins_next)          glyph = f->ini;
            else                          glyph = f->iso;
        } else if (jt == JT_NONE) {
            const ps_form *f = find_form(cp);
            if (f) glyph = f->iso;                    /* e.g. hamza */
        }

        if (n >= cap) return PS_ERR_BUFFER;
        out[n++] = glyph;
    }

    *out_len = n;
    return PS_OK;
}

/* ---- Simplified bidi for a right-to-left paragraph ---- */
enum { BC_R, BC_L, BC_NUM, BC_SEP, BC_NEUTRAL };

static int bidi_class(uint32_t cp)
{
    if ((cp >= 0x0660 && cp <= 0x0669) || (cp >= 0x06F0 && cp <= 0x06F9) ||
        (cp >= '0' && cp <= '9'))
        return BC_NUM;
    if (cp == '.' || cp == ',' || cp == ':' || cp == '/' || cp == 0x066B || cp == 0x066C)
        return BC_SEP;                                /* separators inside numbers */
    if (cp == 0x060C || cp == 0x061B || cp == 0x061F) /* Arabic comma, semicolon, ? */
        return BC_NEUTRAL;
    if ((cp >= 0x0590 && cp <= 0x08FF) || (cp >= 0xFB1D && cp <= 0xFDFF) ||
        (cp >= 0xFE70 && cp <= 0xFEFF))
        return BC_R;
    if ((cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') ||
        (cp >= 0x00C0 && cp <= 0x024F))
        return BC_L;
    return BC_NEUTRAL;
}

static uint32_t mirror(uint32_t cp)
{
    switch (cp) {
    case '(': return ')';  case ')': return '(';
    case '[': return ']';  case ']': return '[';
    case '{': return '}';  case '}': return '{';
    case '<': return '>';  case '>': return '<';
    case 0x00AB: return 0x00BB; case 0x00BB: return 0x00AB; /* guillemets */
    default: return cp;
    }
}

static void reverse_u32(uint32_t *a, size_t from, size_t to_excl)
{
    while (from + 1 < to_excl) {
        uint32_t t = a[from]; a[from] = a[to_excl - 1]; a[to_excl - 1] = t;
        from++; to_excl--;
    }
}

static void reverse_u8(uint8_t *a, size_t len)
{
    size_t i = 0, j = len;
    while (i + 1 < j) { uint8_t t = a[i]; a[i] = a[j - 1]; a[j - 1] = t; i++; j--; }
}

ps_status ps_reorder_visual(uint32_t *buf, size_t len)
{
    uint8_t cls[PS_MAX_LINE];   /* bidi class, then 0 = draw LTR, 1 = draw RTL */
    size_t i;

    if (!buf) return PS_ERR_ARG;
    if (len > PS_MAX_LINE) return PS_ERR_BUFFER;

    for (i = 0; i < len; i++) cls[i] = (uint8_t)bidi_class(buf[i]);

    /* A separator between two digits joins the number ("3.5", "1,000"). */
    for (i = 1; i + 1 < len; i++)
        if (cls[i] == BC_SEP && cls[i - 1] == BC_NUM && cls[i + 1] == BC_NUM)
            cls[i] = BC_NUM;
    for (i = 0; i < len; i++) if (cls[i] == BC_SEP) cls[i] = BC_NEUTRAL;

    /* Numbers following Latin text belong to it ("STM32"). */
    {
        int last_strong = BC_R;                       /* paragraph is RTL */
        for (i = 0; i < len; i++) {
            if (cls[i] == BC_R || cls[i] == BC_L) last_strong = cls[i];
            else if (cls[i] == BC_NUM && last_strong == BC_L) cls[i] = BC_L;
        }
    }

    /* Neutrals become Latin only with Latin on both sides; otherwise RTL. */
    for (i = 0; i < len; ) {
        size_t j, k;
        int before, after, dir;
        if (cls[i] != BC_NEUTRAL) { i++; continue; }
        for (j = i; j < len && cls[j] == BC_NEUTRAL; j++) {}
        before = (i == 0)   ? BC_R : (cls[i - 1] == BC_L ? BC_L : BC_R);
        after  = (j == len) ? BC_R : (cls[j]     == BC_L ? BC_L : BC_R);
        dir = (before == BC_L && after == BC_L) ? BC_L : BC_R;
        for (k = i; k < j; k++) cls[k] = (uint8_t)dir;
        i = j;
    }

    /* 0 = left-to-right run, 1 = right-to-left. */
    for (i = 0; i < len; i++) cls[i] = (cls[i] == BC_R) ? 1u : 0u;

    /* Reverse the whole line, then restore each left-to-right run. */
    reverse_u32(buf, 0, len);
    reverse_u8(cls, len);
    for (i = 0; i < len; ) {
        size_t j;
        if (cls[i] != 0) { buf[i] = mirror(buf[i]); i++; continue; }
        for (j = i; j < len && cls[j] == 0; j++) {}
        reverse_u32(buf, i, j);
        i = j;
    }
    return PS_OK;
}

/* ---- UTF-8 ---- */
ps_status ps_utf8_decode(const char *utf8, uint32_t *out, size_t cap, size_t *out_len)
{
    const unsigned char *s = (const unsigned char *)utf8;
    size_t n = 0;

    if (!utf8 || !out || !out_len) return PS_ERR_ARG;

    while (*s) {
        uint32_t cp;
        int extra, k;
        if      (s[0] < 0x80)           { cp = s[0];        extra = 0; }
        else if ((s[0] & 0xE0) == 0xC0) { cp = s[0] & 0x1F; extra = 1; }
        else if ((s[0] & 0xF0) == 0xE0) { cp = s[0] & 0x0F; extra = 2; }
        else if ((s[0] & 0xF8) == 0xF0) { cp = s[0] & 0x07; extra = 3; }
        else return PS_ERR_UTF8;

        for (k = 1; k <= extra; k++) {
            if ((s[k] & 0xC0) != 0x80) return PS_ERR_UTF8;  /* also catches NUL */
            cp = (cp << 6) | (s[k] & 0x3F);
        }
        /* reject overlong forms, surrogates and out-of-range values */
        if ((extra == 1 && cp < 0x80) || (extra == 2 && cp < 0x800) ||
            (extra == 3 && cp < 0x10000) || cp > 0x10FFFF ||
            (cp >= 0xD800 && cp <= 0xDFFF))
            return PS_ERR_UTF8;

        if (n >= cap) return PS_ERR_BUFFER;
        out[n++] = cp;
        s += extra + 1;
    }
    *out_len = n;
    return PS_OK;
}

ps_status ps_utf8_encode(const uint32_t *cps, size_t len, char *out, size_t cap)
{
    size_t i, n = 0;
    if (!cps || !out) return PS_ERR_ARG;
    for (i = 0; i < len; i++) {
        uint32_t c = cps[i];
        unsigned char b[4];
        size_t k, m;
        if (c < 0x80)         { b[0] = (unsigned char)c; m = 1; }
        else if (c < 0x800)   { b[0] = (unsigned char)(0xC0 | (c >> 6));
                                b[1] = (unsigned char)(0x80 | (c & 0x3F)); m = 2; }
        else if (c < 0x10000) { b[0] = (unsigned char)(0xE0 | (c >> 12));
                                b[1] = (unsigned char)(0x80 | ((c >> 6) & 0x3F));
                                b[2] = (unsigned char)(0x80 | (c & 0x3F)); m = 3; }
        else                  { b[0] = (unsigned char)(0xF0 | (c >> 18));
                                b[1] = (unsigned char)(0x80 | ((c >> 12) & 0x3F));
                                b[2] = (unsigned char)(0x80 | ((c >> 6) & 0x3F));
                                b[3] = (unsigned char)(0x80 | (c & 0x3F)); m = 4; }
        if (n + m + 1 > cap) return PS_ERR_BUFFER;
        for (k = 0; k < m; k++) out[n++] = (char)b[k];
    }
    if (n + 1 > cap) return PS_ERR_BUFFER;
    out[n] = '\0';
    return PS_OK;
}

/* ---- One-call pipeline ---- */
ps_status ps_layout_utf8(const char *utf8, uint32_t *out, size_t cap,
                         size_t *out_len, unsigned flags)
{
    uint32_t logical[PS_MAX_LINE];
    size_t n_logical;
    ps_status st;

    if (!utf8 || !out || !out_len) return PS_ERR_ARG;

    st = ps_utf8_decode(utf8, logical, PS_MAX_LINE, &n_logical);
    if (st != PS_OK) return st;
    st = ps_shape(logical, n_logical, out, cap, out_len, flags);
    if (st != PS_OK) return st;
    return ps_reorder_visual(out, *out_len);
}
