/*
 * ps_shaper.h - Persian text shaping and right-to-left layout for microcontrollers.
 *
 * Turns Persian (and Arabic-script) UTF-8 text into a sequence of Unicode
 * presentation-form code points in visual (left-to-right drawing) order, so a
 * simple bitmap renderer can draw it glyph by glyph on a small display.
 *
 * Design rules:
 *   - C99, no malloc, no global state, no dependencies beyond <stddef.h>/<stdint.h>.
 *   - Callers supply all output buffers.
 *   - One line of text at a time (no line breaking).
 */
#ifndef PS_SHAPER_H
#define PS_SHAPER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Longest line ps_reorder_visual() and ps_layout_utf8() accept (code points).
 * Uses this many bytes of stack, plus 4x this for ps_layout_utf8(). */
#ifndef PS_MAX_LINE
#define PS_MAX_LINE 128
#endif

typedef enum {
    PS_OK          =  0,
    PS_ERR_UTF8    = -1,  /* malformed UTF-8 input                 */
    PS_ERR_BUFFER  = -2,  /* output buffer or PS_MAX_LINE too small */
    PS_ERR_ARG     = -3   /* NULL pointer argument                 */
} ps_status;

/* Flags for ps_shape() and ps_layout_utf8() */
#define PS_FLAG_NORMALISE_ARABIC  0x01u  /* map Arabic kaf/yeh to Persian kaf/yeh */
#define PS_FLAG_KEEP_MARKS        0x02u  /* keep diacritics (default: drop them)   */

/* Decode UTF-8 into code points. */
ps_status ps_utf8_decode(const char *utf8, uint32_t *out, size_t cap, size_t *out_len);

/* Encode code points as UTF-8 (NUL-terminated). */
ps_status ps_utf8_encode(const uint32_t *cps, size_t len, char *out, size_t cap);

/* Contextual shaping: picks isolated/initial/medial/final presentation forms,
 * forms lam-alef ligatures and removes ZWNJ (U+200C) after using it to break
 * joining. Output stays in logical (reading) order and is never longer
 * than the input. */
ps_status ps_shape(const uint32_t *in, size_t len,
                   uint32_t *out, size_t cap, size_t *out_len, unsigned flags);

/* Simplified bidi for a right-to-left line: reverses Persian runs, keeps Latin
 * words and numbers left-to-right, and mirrors brackets inside RTL text.
 * Works in place. */
ps_status ps_reorder_visual(uint32_t *buf, size_t len);

/* Everything in one call: UTF-8 -> shaped code points in visual order. */
ps_status ps_layout_utf8(const char *utf8, uint32_t *out, size_t cap,
                         size_t *out_len, unsigned flags);

#ifdef __cplusplus
}
#endif

#endif /* PS_SHAPER_H */
