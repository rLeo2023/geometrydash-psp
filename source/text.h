#pragma once
#include <3ds.h>
#include <citro2d.h>

typedef struct {
    unsigned short id;
    unsigned short x;
    unsigned short y;
    unsigned short width;
    unsigned short height;
    short xOffset;
    short yOffset;
    short xAdvance;
    short spriteIndex;
} Glyph;

#define HEIGHT_OFFSET (20.f)
#define HEIGHT_OFFSET_MULT (HEIGHT_OFFSET / 29)

typedef struct {
    const Glyph* glyphs;
    unsigned int count;
} Charset;

bool parse_hex_color(const char *str, u32 *out);

void draw_text(const Charset *font, C2D_SpriteSheet *sheet, const float x, const float y, const float scaleX, const float scaleY, float alignment, bool parse_tags, const char *text, ...);
float get_text_length(const Charset *font, const float zoom_x, bool parse_tags, const char *text);
float get_longest_line_length(const Charset *font, const float zoom_x, const char *text);
char *wrap_text(const Charset *font, float zoom_x, const char *text, float max_width);