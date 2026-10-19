#pragma once

#ifdef __PSP__
#include <citro2d.h>

// the user icon's own sheet (one image, frame 0)
C2D_SpriteSheet profile_assets(void);
void profile_assets_free(void);

void profile_init(void);
int profile_loop(void);
#endif
