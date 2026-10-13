/* Host-side tests for ps_shaper. Build and run with: make test */
#include <stdio.h>
#include <string.h>
#include "ps_shaper.h"

static int failures = 0, checks = 0;

static void print_cps(const uint32_t *a, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) printf(" %04X", (unsigned)a[i]);
}

static void expect_seq(const char *name, const uint32_t *got, size_t got_n,
                       const uint32_t *want, size_t want_n)
{
    checks++;
    if (got_n != want_n || memcmp(got, want, want_n * sizeof(uint32_t)) != 0) {
        failures++;
        printf("FAIL %s\n  want:", name); print_cps(want, want_n);
        printf("\n  got: ");              print_cps(got, got_n);
        printf("\n");
    } else {
        printf("ok   %s\n", name);
    }
}

/* Shape only (logical order). */
static void check_shape(const char *name, const char *utf8, unsigned flags,
                        const uint32_t *want, size_t want_n)
{
    uint32_t in[PS_MAX_LINE], out[PS_MAX_LINE];
    size_t n_in = 0, n_out = 0;
    if (ps_utf8_decode(utf8, in, PS_MAX_LINE, &n_in) != PS_OK ||
        ps_shape(in, n_in, out, PS_MAX_LINE, &n_out, flags) != PS_OK) {
        checks++; failures++; printf("FAIL %s (error)\n", name); return;
    }
    expect_seq(name, out, n_out, want, want_n);
}

/* Full pipeline (visual order). */
static void check_layout(const char *name, const char *utf8, unsigned flags,
                         const uint32_t *want, size_t want_n)
{
    uint32_t out[PS_MAX_LINE];
    size_t n = 0;
    if (ps_layout_utf8(utf8, out, PS_MAX_LINE, &n, flags) != PS_OK) {
        checks++; failures++; printf("FAIL %s (error)\n", name); return;
    }
    expect_seq(name, out, n, want, want_n);
}

#define N(a) (sizeof(a) / sizeof((a)[0]))

int main(void)
{
    /* --- shaping (logical order) --- */
    {   /* peh initial, dal final, reh isolated */
        static const uint32_t w[] = {0xFB58, 0xFEAA, 0xFEAD};
        check_shape("peh/dal/reh forms", "پدر", 0, w, N(w));
    }
    {   /* seen initial, lam-alef final ligature, meem isolated */
        static const uint32_t w[] = {0xFEB3, 0xFEFC, 0xFEE1};
        check_shape("lam-alef ligature", "سلام", 0, w, N(w));
    }
    {
        static const uint32_t w[] = {0xFEFB};
        check_shape("isolated lam-alef", "لا", 0, w, N(w));
    }
    {   /* ZWNJ: yeh must be final, khah initial, ZWNJ removed */
        static const uint32_t w[] = {0xFEE3, 0xFBFD, 0xFEA7, 0xFEEE,
                                     0xFE8D, 0xFEEB, 0xFEE2};
        check_shape("ZWNJ breaks joining", "می\xE2\x80\x8Cخواهم", 0, w, N(w));
    }
    {
        static const uint32_t w[] = {0xFB90, 0xFE98, 0xFE8E, 0xFE8F, ' ',
                                     0xFB94, 0xFB7B, ' ', 0xFB8A};
        check_shape("Persian letters", "کتاب گچ ژ", 0, w, N(w));
    }
    {
        static const uint32_t w[] = {0xFB90, 0xFBFD};
        check_shape("normalise Arabic kaf/yeh", "كي", PS_FLAG_NORMALISE_ARABIC, w, N(w));
    }
    {   /* diacritic is transparent: still joins, mark dropped */
        static const uint32_t w[] = {0xFE91, 0xFE90};
        check_shape("marks are transparent", "ب\xD9\x8Eب", 0, w, N(w));
    }
    {
        static const uint32_t w[] = {0xFE8F, 0xFE80, 0xFE8F};
        check_shape("hamza non-joining", "بءب", 0, w, N(w));
    }

    /* --- full layout (visual order) --- */
    {
        static const uint32_t w[] = {0xFEE1, 0xFEFC, 0xFEB3};
        check_layout("RTL word reversed", "سلام", 0, w, N(w));
    }
    {
        static const uint32_t w[] = {'2', '0', '2', '6', ' ', 0xFEDD, 0xFE8E, 0xFEB3};
        check_layout("number inside RTL", "سال 2026", 0, w, N(w));
    }
    {
        static const uint32_t w[] = {0xFEE1, 0xFEAD, 0xFE8D, 0xFEA9, ' ',
                                     'S', 'T', 'M', '3', '2', ' ', 0xFEE6, 0xFEE3};
        check_layout("Latin word inside RTL", "من STM32 دارم", 0, w, N(w));
    }
    {
        static const uint32_t w[] = {'(', 0xFEE1, 0xFEFC, 0xFEB3, ')'};
        check_layout("brackets mirrored", "(سلام)", 0, w, N(w));
    }
    {
        static const uint32_t w[] = {0x06F3, '.', 0x06F5};
        check_layout("Persian decimal number", "۳.۵", 0, w, N(w));
    }

    /* --- error handling --- */
    {
        uint32_t out[4]; size_t n;
        checks++;
        if (ps_utf8_decode("\xC0\x80", out, 4, &n) != PS_ERR_UTF8) {
            failures++; printf("FAIL overlong UTF-8 rejected\n");
        } else printf("ok   overlong UTF-8 rejected\n");
        checks++;
        if (ps_layout_utf8("سلام دنیا", out, 4, &n, 0) != PS_ERR_BUFFER) {
            failures++; printf("FAIL small buffer reported\n");
        } else printf("ok   small buffer reported\n");
    }

    printf("\n%d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
