/* ps_demo - shape a line of Persian text and print the glyph sequence.
 * Usage: ./build/ps_demo "سلام دنیا"
 * Your terminal applies its own bidi, so the UTF-8 line may look reversed;
 * the hex list is the ground truth. */
#include <stdio.h>
#include "ps_shaper.h"

int main(int argc, char **argv)
{
    uint32_t glyphs[PS_MAX_LINE];
    char utf8[PS_MAX_LINE * 4 + 1];
    size_t n = 0, i;
    ps_status st;

    if (argc < 2) {
        fprintf(stderr, "usage: %s \"<persian text>\"\n", argv[0]);
        return 2;
    }
    st = ps_layout_utf8(argv[1], glyphs, PS_MAX_LINE, &n, PS_FLAG_NORMALISE_ARABIC);
    if (st != PS_OK) {
        fprintf(stderr, "error %d\n", (int)st);
        return 1;
    }
    printf("%zu glyphs, left to right:\n", n);
    for (i = 0; i < n; i++) printf("U+%04X%s", (unsigned)glyphs[i], (i + 1) % 8 ? " " : "\n");
    printf("\n");
    if (ps_utf8_encode(glyphs, n, utf8, sizeof utf8) == PS_OK)
        printf("as UTF-8: %s\n", utf8);
    return 0;
}
