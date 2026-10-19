#pragma once

// The UI's language: English as written in the layouts and sources, or a
// Spanish translation of it. The fonts have no accents (bigFont is ASCII),
// so the Spanish strings are written without them.

enum {
    LANG_ENGLISH = 0,
    LANG_SPANISH = 1,
    LANG_ITALIAN = 2,
    LANG_PORTUGUESE = 3,
};

extern int language;

struct UIScreen;

// A source string to show in the current language
const char *tr(const char *english);

// Switch language; screens already loaded are updated by their menu loop
void i18n_set_language(int lang);

// Translate every label/button/textbox of a screen (both ways, so loading a
// screen while in Spanish and later switching back also works)
void i18n_localize_screen(struct UIScreen *screen);
