/*
triggers_v2.c - the triggers GD 2.0 added, for the PSP build

Groups: an object can be in a few of them (its key 57, "1.2.3"); the 2.0
triggers act on a group (their key 51):

  move   (901)   moves the group by (28, 29) over a time (10), eased (30, 85);
                 or follows the player's x/y for that time (58, 59)
  alpha  (1007)  fades the group to an opacity (35) over a time (10)
  toggle (1049)  turns the group off or back on (56): off, it isn't drawn,
                 doesn't collide, and its triggers don't run
  spawn  (1268)  after a delay (63), runs the group's triggers that only a
                 spawn starts (62)
  pulse  (1006)  a color over a channel or a group (52): in (45), held (46),
                 out (47); a color (7, 8, 9) or an HSV change (48, 49) of a
                 channel's color (50)

A group's movement is kept as its total offset: an object is where the
level put it plus its groups' offsets, so going back (a death, a practice
checkpoint) puts everything where it was. How these behave follows OpenGD
(github.com/Open-GD/OpenGD) and the decompiled GD 2.113.
*/
#ifdef __PSP__
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "triggers_v2.h"
#include "color.h"
#include "level_loading.h"
#include "main.h"
#include "objects.h"
#include "state.h"
#include "player/collision.h"

#define MAX_GROUP   1000
#define MAX_MOVES   128
#define MAX_SPAWNS  64
#define MAX_PULSES  64

#define TRIG_MOVE   901
#define TRIG_PULSE  1006
#define TRIG_ALPHA  1007
#define TRIG_TOGGLE 1049
#define TRIG_SPAWN  1268

void run_trigger(int obj);                          // color_channels.c
void move_object_section(int obj, float old_x, float old_y); // level_loading.c

typedef struct {
    u16 target;          // 51
    u8 easing;           // 30
    u8 pulse_mode;       // 48: 0 a color, 1 an HSV change
    u8 pulse_target;     // 52: 0 a color channel, 1 a group
    bool lock_x, lock_y; // 58, 59
    bool main_only, detail_only; // 65, 66
    bool activate;       // 56
    bool spawn_only;     // 62
    bool s_add, v_add;   // 49's 4th and 5th
    float dx, dy;        // 28, 29
    float rate;          // 85
    float opacity;       // 35
    float fade_in, hold, fade_out; // 45, 46, 47
    float spawn_delay;   // 63
    float h, s, v;       // 49
    int copied;          // 50
} Trig;

typedef struct {
    float dx, dy;        // the group's offset so far
    float alpha, a_from, a_to, a_t, a_dur;
    bool a_active;
    bool off;
} Group;

typedef struct {
    bool active;
    u8 easing;
    bool lock_x, lock_y;
    u16 group;
    float dx, dy, dur, t, done_x, done_y, rate;
} Move;

typedef struct {
    bool active;
    u16 group;
    float left;
} Spawn;

typedef struct {
    bool active;
    u8 target_type;
    bool hsv, s_add, v_add, main_only, detail_only;
    u16 target;
    int copied;
    Color color;
    float h, s, v;
    float fade_in, hold, fade_out, t;
} Pulse;

typedef struct {
    int max_group;      // the level's highest
    Group groups[MAX_GROUP];
    Move moves[MAX_MOVES];
    Spawn spawns[MAX_SPAWNS];
    Pulse pulses[MAX_PULSES];
    float player_x, player_y; // last frame's, for the moves that follow it
} State;

static State st;

float *trig2_obj_scale;

static Trig *trigs;
static int ntrigs, trigs_cap;
static int *trig_of;            // object -> trigs[] or -1
static int nobjs;

// the groups each object is in, and the objects of each group
static int *og_start;
static u8 *og_count;
static u16 *og_data;
static int og_len, og_cap;
static int gm_start[MAX_GROUP + 1];
static int *gm_data;
static float *orig_x, *orig_y;  // where the level put them (grouped objects)

// channels a pulse changed this frame, and their own colors
static int pulsed[MAX_PULSES];
static Color pulsed_base[MAX_PULSES];
static int npulsed;

/* ---- loading -------------------------------------------------------- */

static void ensure_arrays(void) {
    if (trig_of && nobjs == objects.count) return;
    trig2_free();
    nobjs = objects.count;
    trig_of = malloc(sizeof(int) * nobjs);
    og_start = calloc(nobjs, sizeof(int));
    og_count = calloc(nobjs, sizeof(u8));
    trig2_obj_scale = malloc(sizeof(float) * nobjs);
    for (int i = 0; trig_of && i < nobjs; i++) trig_of[i] = -1;
    for (int i = 0; trig2_obj_scale && i < nobjs; i++) trig2_obj_scale[i] = 1.f;
}

static bool is_v2_trigger(int id) {
    return id == TRIG_MOVE || id == TRIG_PULSE || id == TRIG_ALPHA || id == TRIG_TOGGLE || id == TRIG_SPAWN;
}

// color triggers: run by color_channels.c, their 2.0 opacity and copy from here
static bool is_color_trigger(int id) {
    switch (id) {
        case BG_TRIGGER: case GROUND_TRIGGER: case LINE_TRIGGER: case V2_0_LINE_TRIGGER:
        case OBJ_TRIGGER: case OBJ_2_TRIGGER: case COL2_TRIGGER: case COL3_TRIGGER:
        case COL4_TRIGGER: case THREEDL_TRIGGER: case COL_TRIGGER: case 900:
            return true;
    }
    return false;
}

static bool has_params(int id) {
    return is_v2_trigger(id) || is_color_trigger(id);
}

static Trig *new_trig(int obj) {
    if (ntrigs == trigs_cap) {
        trigs_cap = trigs_cap ? trigs_cap * 2 : 256;
        trigs = realloc(trigs, sizeof(Trig) * trigs_cap);
    }
    Trig *t = &trigs[ntrigs];
    memset(t, 0, sizeof(*t));
    t->rate = 2.f;
    t->opacity = 1.f;
    t->copied = -1;
    t->s = t->v = 1.f;
    trig_of[obj] = ntrigs++;
    return t;
}

void trig2_parse_object(int obj, char **tokens, int count) {
    ensure_arrays();
    if (!trig_of || !og_start || !og_count) return;

    int id = objects.id[obj];
    Trig *t = NULL;
    bool spawn_only = false;

    for (int i = 0; i + 1 < count; i += 2) {
        int key = atoi(tokens[i]);
        const char *v = tokens[i + 1];

        if (key == 57) {
            // its groups
            og_start[obj] = og_len;
            const char *p = v;
            while (*p && og_count[obj] < MAX_GROUPS_PER_OBJECT) {
                int g = atoi(p);
                if (g > 0 && g < MAX_GROUP) {
                    if (og_len == og_cap) {
                        og_cap = og_cap ? og_cap * 2 : 4096;
                        og_data = realloc(og_data, sizeof(u16) * og_cap);
                    }
                    og_data[og_len++] = g;
                    og_count[obj]++;
                    if (g > st.max_group) st.max_group = g;
                }
                while (*p && *p != '.') p++;
                if (*p == '.') p++;
            }
            continue;
        }
        if (key == 62) spawn_only = atoi(v) != 0;
        if (key == 32 && trig2_obj_scale) {
            // its size: drawn and collides that big
            float sc = atof(v);
            if (sc > 0 && sc != 1.f) {
                trig2_obj_scale[obj] = sc;
                objects.width[obj] *= sc;
                objects.height[obj] *= sc;
            }
        }
    }

    // any trigger can be spawn-only (color ones too): keep that
    if (!has_params(id) && !spawn_only) return;
    t = new_trig(obj);
    t->spawn_only = spawn_only;
    if (!has_params(id)) return;

    for (int i = 0; i + 1 < count; i += 2) {
        int key = atoi(tokens[i]);
        const char *v = tokens[i + 1];
        switch (key) {
            case 51: t->target = (u16)atoi(v); col_note_used(t->target); break;
            case 28: t->dx = atof(v); break;
            case 29: t->dy = atof(v); break;
            case 30: t->easing = (u8)atoi(v); break;
            case 85: t->rate = atof(v); break;
            case 58: t->lock_x = atoi(v) != 0; break;
            case 59: t->lock_y = atoi(v) != 0; break;
            case 35: t->opacity = atof(v); break;
            case 45: t->fade_in = atof(v); break;
            case 46: t->hold = atof(v); break;
            case 47: t->fade_out = atof(v); break;
            case 48: t->pulse_mode = (u8)atoi(v); break;
            case 52: t->pulse_target = (u8)atoi(v); break;
            case 50: t->copied = atoi(v); break;
            case 56: t->activate = atoi(v) != 0; break;
            case 63: t->spawn_delay = atof(v); break;
            case 65: t->main_only = atoi(v) != 0; break;
            case 66: t->detail_only = atoi(v) != 0; break;
            case 49: {
                // "hue a saturation a value a s_add a v_add"
                float f[5] = { 0, 1, 1, 0, 0 };
                const char *p = v;
                for (int k = 0; k < 5 && *p; k++) {
                    f[k] = atof(p);
                    while (*p && *p != 'a') p++;
                    if (*p == 'a') p++;
                }
                t->h = f[0]; t->s = f[1]; t->v = f[2];
                t->s_add = f[3] != 0; t->v_add = f[4] != 0;
                break;
            }
        }
    }
}

void trig2_level_loaded(void) {
    if (!og_count) return;

    // the objects of each group
    memset(gm_start, 0, sizeof(gm_start));
    for (int o = 0; o < nobjs; o++)
        for (int k = 0; k < og_count[o]; k++)
            gm_start[og_data[og_start[o] + k] + 1]++;
    for (int g = 0; g < MAX_GROUP; g++)
        gm_start[g + 1] += gm_start[g];
    free(gm_data);
    gm_data = malloc(sizeof(int) * (gm_start[MAX_GROUP] + 1));
    int fill[MAX_GROUP];
    memcpy(fill, gm_start, sizeof(fill));
    for (int o = 0; o < nobjs; o++)
        for (int k = 0; k < og_count[o]; k++)
            gm_data[fill[og_data[og_start[o] + k]]++] = o;

    free(orig_x);
    free(orig_y);
    orig_x = malloc(sizeof(float) * nobjs);
    orig_y = malloc(sizeof(float) * nobjs);
    if (orig_x && orig_y) {
        memcpy(orig_x, objects.x, sizeof(float) * nobjs);
        memcpy(orig_y, objects.y, sizeof(float) * nobjs);
    }

    trig2_reset();
}

void trig2_free(void) {
    free(trigs); trigs = NULL; ntrigs = trigs_cap = 0;
    free(trig_of); trig_of = NULL;
    free(og_start); og_start = NULL;
    free(og_count); og_count = NULL;
    free(og_data); og_data = NULL; og_len = og_cap = 0;
    free(gm_data); gm_data = NULL;
    free(orig_x); orig_x = NULL;
    free(orig_y); orig_y = NULL;
    free(trig2_obj_scale); trig2_obj_scale = NULL;
    memset(gm_start, 0, sizeof(gm_start));
    nobjs = 0;
    st.max_group = 0;
}

/* ---- group state ---------------------------------------------------- */

static bool is_trigger_obj(int obj) {
    int id = objects.id[obj];
    return id > 0 && id < GAME_OBJECT_COUNT && game_objects[id].texture < 0;
}

// every grouped object where its groups' offsets put it, and off when one
// of its groups is
static void place_objects(void) {
    if (!og_count || !orig_x) return;
    for (int o = 0; o < nobjs; o++) {
        if (!og_count[o]) continue;
        float dx = 0, dy = 0;
        bool off = false;
        for (int k = 0; k < og_count[o]; k++) {
            Group *g = &st.groups[og_data[og_start[o] + k]];
            dx += g->dx;
            dy += g->dy;
            off |= g->off;
        }
        objects.toggled[o] = off;
        if (is_trigger_obj(o)) continue;
        float x = orig_x[o] + dx, y = orig_y[o] + dy;
        if (x != objects.x[o] || y != objects.y[o]) {
            float ox = objects.x[o], oy = objects.y[o];
            objects.x[o] = x;
            objects.y[o] = y;
            move_object_section(o, ox, oy);
        }
    }
}

void trig2_reset(void) {
    int max_group = st.max_group;
    memset(&st, 0, sizeof(st));
    st.max_group = max_group;
    for (int g = 0; g < MAX_GROUP; g++) st.groups[g].alpha = 1.f;
    st.player_x = state.player.x;
    st.player_y = state.player.y;
    npulsed = 0;
    place_objects();
}

static void move_group(int g, float dx, float dy) {
    if ((!dx && !dy) || g <= 0 || g >= MAX_GROUP || !gm_data) return;
    st.groups[g].dx += dx;
    st.groups[g].dy += dy;
    for (int i = gm_start[g]; i < gm_start[g + 1]; i++) {
        int o = gm_data[i];
        if (is_trigger_obj(o)) continue;
        float ox = objects.x[o], oy = objects.y[o];
        objects.x[o] += dx;
        objects.y[o] += dy;
        move_object_section(o, ox, oy);
    }
}

static void toggle_group(int g, bool on) {
    if (g <= 0 || g >= MAX_GROUP || !gm_data) return;
    st.groups[g].off = !on;
    for (int i = gm_start[g]; i < gm_start[g + 1]; i++) {
        int o = gm_data[i];
        bool off = false;
        for (int k = 0; k < og_count[o]; k++)
            off |= st.groups[og_data[og_start[o] + k]].off;
        objects.toggled[o] = off;
    }
}

/* ---- easing (cocos2d's, which GD uses) ------------------------------ */

static float bounce_out(float t) {
    if (t < 1 / 2.75f) return 7.5625f * t * t;
    if (t < 2 / 2.75f) { t -= 1.5f / 2.75f; return 7.5625f * t * t + 0.75f; }
    if (t < 2.5f / 2.75f) { t -= 2.25f / 2.75f; return 7.5625f * t * t + 0.9375f; }
    t -= 2.625f / 2.75f;
    return 7.5625f * t * t + 0.984375f;
}

static float ease(int type, float rate, float t) {
    const float pi = 3.14159265f;
    float p;
    switch (type) {
        case 1: // ease in out
            t *= 2;
            return t < 1 ? 0.5f * powf(t, rate) : 1.f - 0.5f * powf(2 - t, rate);
        case 2: return powf(t, rate);           // ease in
        case 3: return powf(t, 1.f / rate);     // ease out
        case 4: // elastic in out
            if (t == 0 || t == 1) return t;
            p = rate > 0 ? rate : 0.3f * 1.5f;
            t = t * 2 - 1;
            if (t < 0) return -0.5f * powf(2, 10 * t) * sinf((t - p / 4) * 2 * pi / p);
            return powf(2, -10 * t) * sinf((t - p / 4) * 2 * pi / p) * 0.5f + 1;
        case 5: // elastic in
            if (t == 0 || t == 1) return t;
            p = rate > 0 ? rate : 0.3f;
            t -= 1;
            return -powf(2, 10 * t) * sinf((t - p / 4) * 2 * pi / p);
        case 6: // elastic out
            if (t == 0 || t == 1) return t;
            p = rate > 0 ? rate : 0.3f;
            return powf(2, -10 * t) * sinf((t - p / 4) * 2 * pi / p) + 1;
        case 7: // bounce in out
            return t < 0.5f ? (1 - bounce_out(1 - t * 2)) * 0.5f : bounce_out(t * 2 - 1) * 0.5f + 0.5f;
        case 8: return 1 - bounce_out(1 - t);  // bounce in
        case 9: return bounce_out(t);          // bounce out
        case 10: // exponential in out
            if (t == 0 || t == 1) return t;
            t *= 2;
            return t < 1 ? 0.5f * powf(2, 10 * (t - 1)) : 0.5f * (-powf(2, -10 * (t - 1)) + 2);
        case 11: return t == 0 ? 0 : powf(2, 10 * (t - 1)) - 0.001f; // exponential in
        case 12: return t == 1 ? 1 : -powf(2, -10 * t) + 1;           // exponential out
        case 13: return -0.5f * (cosf(pi * t) - 1);                    // sine in out
        case 14: return -cosf(t * pi / 2) + 1;                         // sine in
        case 15: return sinf(t * pi / 2);                              // sine out
        case 16: { // back in out
            float o = 1.70158f * 1.525f;
            t *= 2;
            if (t < 1) return (t * t * ((o + 1) * t - o)) / 2;
            t -= 2;
            return (t * t * ((o + 1) * t + o)) / 2 + 1;
        }
        case 17: { float o = 1.70158f; return t * t * ((o + 1) * t - o); } // back in
        case 18: { float o = 1.70158f; t -= 1; return t * t * ((o + 1) * t + o) + 1; } // back out
    }
    return t;
}

/* ---- running triggers ----------------------------------------------- */

bool trig2_color_info(int obj, float *opacity, int *copied, float *h, float *s, float *v, bool *s_add, bool *v_add) {
    if (!trig_of || obj < 0 || obj >= nobjs || trig_of[obj] < 0) return false;
    const Trig *t = &trigs[trig_of[obj]];
    *opacity = t->opacity;
    *copied = t->copied;
    *h = t->h; *s = t->s; *v = t->v;
    *s_add = t->s_add; *v_add = t->v_add;
    return true;
}

bool trig2_spawn_only(int obj) {
    return trig_of && trig_of[obj] >= 0 && trigs[trig_of[obj]].spawn_only;
}

bool trig2_run(int obj) {
    if (!trig_of || trig_of[obj] < 0) return false;
    Trig *t = &trigs[trig_of[obj]];
    int i;

    switch (objects.id[obj]) {
        case TRIG_MOVE:
            for (i = 0; i < MAX_MOVES && st.moves[i].active; i++);
            if (i == MAX_MOVES) return true;
            if (objects.trig_duration[obj] <= 0 && !t->lock_x && !t->lock_y) {
                move_group(t->target, t->dx, t->dy);
                return true;
            }
            st.moves[i] = (Move){ .active = true, .easing = t->easing, .lock_x = t->lock_x, .lock_y = t->lock_y,
                                  .group = t->target, .dx = t->dx, .dy = t->dy,
                                  .dur = objects.trig_duration[obj], .rate = t->rate };
            return true;

        case TRIG_ALPHA:
            if (t->target > 0 && t->target < MAX_GROUP) {
                Group *g = &st.groups[t->target];
                g->a_from = g->alpha;
                g->a_to = t->opacity;
                g->a_t = 0;
                g->a_dur = objects.trig_duration[obj];
                g->a_active = g->a_dur > 0;
                if (!g->a_active) g->alpha = t->opacity;
            }
            return true;

        case TRIG_TOGGLE:
            toggle_group(t->target, t->activate);
            return true;

        case TRIG_SPAWN:
            for (i = 0; i < MAX_SPAWNS && st.spawns[i].active; i++);
            if (i < MAX_SPAWNS) st.spawns[i] = (Spawn){ true, t->target, t->spawn_delay };
            return true;

        case TRIG_PULSE:
            for (i = 0; i < MAX_PULSES && st.pulses[i].active; i++);
            if (i == MAX_PULSES) return true;
            st.pulses[i] = (Pulse){
                .active = true, .target_type = t->pulse_target, .hsv = t->pulse_mode == 1,
                .s_add = t->s_add, .v_add = t->v_add, .main_only = t->main_only, .detail_only = t->detail_only,
                .target = t->target, .copied = t->copied,
                .color = { objects.trig_colorR[obj], objects.trig_colorG[obj], objects.trig_colorB[obj] },
                .h = t->h, .s = t->s, .v = t->v,
                .fade_in = t->fade_in, .hold = t->hold, .fade_out = t->fade_out,
            };
            return true;
    }
    return false;
}

static void run_spawn(int g) {
    if (g <= 0 || g >= MAX_GROUP || !gm_data) return;
    for (int i = gm_start[g]; i < gm_start[g + 1]; i++) {
        int o = gm_data[i];
        if (trig_of[o] >= 0 && trigs[trig_of[o]].spawn_only && !objects.toggled[o])
            run_trigger(o);
    }
}

void trig2_update(float dt) {
    float pdx = state.player.x - st.player_x;
    float pdy = state.player.y - st.player_y;
    st.player_x = state.player.x;
    st.player_y = state.player.y;
    if (!gm_data) return;

    for (int i = 0; i < MAX_MOVES; i++) {
        Move *m = &st.moves[i];
        if (!m->active) continue;
        m->t += dt;
        float p = m->dur > 0 ? m->t / m->dur : 1;
        if (p > 1) p = 1;
        float e = ease(m->easing, m->rate, p);
        float tx = m->dx * e, ty = m->dy * e;
        float ddx = tx - m->done_x, ddy = ty - m->done_y;
        m->done_x = tx;
        m->done_y = ty;
        if (m->lock_x) ddx += pdx;
        if (m->lock_y) ddy += pdy;
        move_group(m->group, ddx, ddy);
        if (p >= 1) m->active = false;
    }

    for (int g = 1; g <= st.max_group; g++) {
        Group *gr = &st.groups[g];
        if (!gr->a_active) continue;
        gr->a_t += dt;
        float p = gr->a_t / gr->a_dur;
        if (p >= 1) { p = 1; gr->a_active = false; }
        gr->alpha = gr->a_from + (gr->a_to - gr->a_from) * p;
    }

    for (int i = 0; i < MAX_SPAWNS; i++) {
        Spawn *s = &st.spawns[i];
        if (!s->active) continue;
        s->left -= dt;
        if (s->left <= 0) {
            s->active = false;
            run_spawn(s->group);
        }
    }

    for (int i = 0; i < MAX_PULSES; i++) {
        Pulse *p = &st.pulses[i];
        if (!p->active) continue;
        p->t += dt;
        if (p->t >= p->fade_in + p->hold + p->fade_out) p->active = false;
    }
}

/* ---- pulses --------------------------------------------------------- */

static float pulse_strength(const Pulse *p) {
    if (p->t < p->fade_in) return p->fade_in > 0 ? p->t / p->fade_in : 1;
    if (p->t < p->fade_in + p->hold) return 1;
    if (p->fade_out <= 0) return 0;
    float s = 1 - (p->t - p->fade_in - p->hold) / p->fade_out;
    return s < 0 ? 0 : s;
}

static Color apply_hsv(Color c, const Pulse *p) {
    return col_apply_hsv(c, p->h, p->s, p->v, p->s_add, p->v_add);
}

static Color lerp_color(Color a, Color b, float k) {
    Color c;
    c.r = (unsigned char)(a.r + (b.r - a.r) * k);
    c.g = (unsigned char)(a.g + (b.g - a.g) * k);
    c.b = (unsigned char)(a.b + (b.b - a.b) * k);
    return c;
}

// what a pulse turns a color into, at full strength
static Color pulse_target(const Pulse *p, Color own) {
    if (!p->hsv) return p->color;
    Color src = own;
    if (p->copied > 0) src = channels[get_col_channel_index(p->copied)].color;
    return apply_hsv(src, p);
}

void trig2_unpulse_channels(void) {
    for (int i = npulsed - 1; i >= 0; i--)
        channels[pulsed[i]].color = pulsed_base[i];
    npulsed = 0;
}

void trig2_pulse_channels(void) {
    col_apply_copies(); // copied channels first: pulses go on top
    for (int i = 0; i < MAX_PULSES; i++) {
        const Pulse *p = &st.pulses[i];
        if (!p->active || p->target_type != 0) continue;
        int idx = get_col_channel_index(p->target);
        if (npulsed < MAX_PULSES) {
            pulsed[npulsed] = idx;
            pulsed_base[npulsed] = channels[idx].color;
            npulsed++;
        }
        Color own = channels[idx].color;
        channels[idx].color = lerp_color(own, pulse_target(p, own), pulse_strength(p));
    }
}

void trig2_object_color(int obj, int col_type, Color *color, float *opacity) {
    if (!og_count || obj < 0 || obj >= nobjs || !og_count[obj]) return;

    for (int k = 0; k < og_count[obj]; k++) {
        int g = og_data[og_start[obj] + k];
        *opacity *= st.groups[g].alpha;

        for (int i = 0; i < MAX_PULSES; i++) {
            const Pulse *p = &st.pulses[i];
            if (!p->active || p->target_type != 1 || p->target != g) continue;
            bool is_main = col_type == COLOR_TYPE_BASE;
            bool is_detail = col_type == COLOR_TYPE_DETAIL;
            if (p->main_only && !is_main) continue;
            if (p->detail_only && !is_detail) continue;
            if (!p->main_only && !p->detail_only && !is_main && !is_detail) continue;
            *color = lerp_color(*color, pulse_target(p, *color), pulse_strength(p));
        }
    }
}

/* ---- practice checkpoints ------------------------------------------- */

// only the groups the level uses (up to its highest), not all MAX_GROUP:
// there can be a hundred checkpoints
typedef struct {
    Move moves[MAX_MOVES];
    Spawn spawns[MAX_SPAWNS];
    Pulse pulses[MAX_PULSES];
    int ngroups;
    Group groups[];
} Snapshot;

void *trig2_save(void) {
    int n = st.max_group + 1;
    Snapshot *s = malloc(sizeof(Snapshot) + sizeof(Group) * n);
    if (!s) return NULL;
    memcpy(s->moves, st.moves, sizeof(st.moves));
    memcpy(s->spawns, st.spawns, sizeof(st.spawns));
    memcpy(s->pulses, st.pulses, sizeof(st.pulses));
    s->ngroups = n;
    memcpy(s->groups, st.groups, sizeof(Group) * n);
    return s;
}

void trig2_restore(const void *snapshot) {
    if (!snapshot) {
        trig2_reset();
        return;
    }
    const Snapshot *s = snapshot;
    trig2_reset();
    memcpy(st.moves, s->moves, sizeof(st.moves));
    memcpy(st.spawns, s->spawns, sizeof(st.spawns));
    memcpy(st.pulses, s->pulses, sizeof(st.pulses));
    memcpy(st.groups, s->groups, sizeof(Group) * (s->ngroups < MAX_GROUP ? s->ngroups : MAX_GROUP));
    st.player_x = state.player.x;
    st.player_y = state.player.y;
    npulsed = 0;
    place_objects();

    // the triggers ahead run again (the colors went back too)
    for (int o = 0; o < nobjs; o++) {
        if (is_trigger_obj(o) && objects.x[o] >= state.player.x)
            objects.activated[o] = 0;
    }
}

void trig2_free_snapshot(void *snapshot) {
    free(snapshot);
}
#endif
