#include "ui_on_air.h"

#include "display_init.h"
#include "paddle_input.h"

#include "esp_log.h"

#define FRAME_WIDTH_PX 3
#define POLL_PERIOD_MS 100 /* picks up changes made outside the UI, e.g. CLI `tx` */
#define MAX_TRACKED 6

static const char *TAG = "ui_on_air";

typedef struct {
    lv_obj_t *obj;
    lv_obj_t *label; /* NULL: no text to swap */
    const char *on_text;
    const char *off_text;
} tracked_t;

static lv_obj_t *s_frame;
static tracked_t s_tracked[MAX_TRACKED];
static size_t s_tracked_count;
static bool s_shown_on_air;

static lv_color_t on_air_red(void)
{
    return display_compensate_color(lv_palette_main(LV_PALETTE_RED));
}

static void apply(bool on_air)
{
    if (on_air) {
        lv_obj_remove_flag(s_frame, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_frame, LV_OBJ_FLAG_HIDDEN);
    }
    for (size_t i = 0; i < s_tracked_count; ++i) {
        const tracked_t *t = &s_tracked[i];
        if (on_air) {
            lv_obj_add_state(t->obj, LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(t->obj, LV_STATE_CHECKED);
        }
        if (t->label != NULL) {
            lv_label_set_text(t->label, on_air ? t->on_text : t->off_text);
        }
    }
    s_shown_on_air = on_air;
}

void ui_on_air_refresh(void)
{
    apply(paddle_input_is_on_air());
}

static void poll_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    bool on_air = paddle_input_is_on_air();
    if (on_air != s_shown_on_air) {
        apply(on_air);
    }
}

void ui_on_air_init(void)
{
    /* The system layer sits above every screen and the top layer (where
     * message boxes go), so the frame shows everywhere. It isn't
     * clickable, so touches pass through to whatever is underneath. */
    s_frame = lv_obj_create(lv_layer_sys());
    lv_obj_remove_style_all(s_frame);
    lv_obj_remove_flag(s_frame, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_frame, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s_frame, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(s_frame, on_air_red(), 0);
    lv_obj_set_style_border_width(s_frame, FRAME_WIDTH_PX, 0);
    lv_obj_set_style_border_opa(s_frame, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_frame, LV_OBJ_FLAG_HIDDEN);

    lv_timer_create(poll_timer_cb, POLL_PERIOD_MS, NULL);
    ui_on_air_refresh();
}

static void track(lv_obj_t *obj, lv_obj_t *label, const char *on_text, const char *off_text)
{
    if (s_tracked_count >= MAX_TRACKED) {
        ESP_LOGW(TAG, "too many on-air controls (max %d): one won't show the on-air state", MAX_TRACKED);
        return;
    }
    s_tracked[s_tracked_count++] = (tracked_t){obj, label, on_text, off_text};
    ui_on_air_refresh(); /* bring the new one in step now */
}

void ui_on_air_track(lv_obj_t *obj)
{
    track(obj, NULL, NULL, NULL);
}

static void toggle_cb(lv_event_t *e)
{
    (void)e;
    paddle_input_set_on_air(!paddle_input_is_on_air());
    ui_on_air_refresh();
}

void ui_on_air_add_toggle(lv_obj_t *btn, lv_obj_t *label, const char *on_text, const char *off_text)
{
    static lv_style_t s_checked_style;
    static bool s_checked_style_inited;
    if (!s_checked_style_inited) {
        lv_style_init(&s_checked_style);
        lv_style_set_bg_color(&s_checked_style, on_air_red());
        s_checked_style_inited = true;
    }
    lv_obj_add_style(btn, &s_checked_style, LV_STATE_CHECKED);
    lv_obj_add_event_cb(btn, toggle_cb, LV_EVENT_CLICKED, NULL);
    track(btn, label, on_text, off_text);
}
