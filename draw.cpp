static float global_time;
#define INTERVAL(_interval) for (static float last_t; \
                                 !last_t || global_time > last_t + (_interval); \
                                 last_t = global_time)

struct rect { vec2 min, max; };

rect RectWithPosAndSize(float x, float y, float w, float h) {
    return rect {
        .min = { x, y },
        .max = { x + w, y + h },
    };
}

rgba32 Color(float r, float g, float b, float a = 1);
font* Font(const char* file_name);
image Image(const char* file_name);

float random_float_between_0_and_1();

void reset_transform();
void push_transform(mat3 t);
void pop_transform();
mat3 get_transform();

void draw_rect(float x, float y, float w, float h, rgba32 color);
void draw_ellipse(float x, float y, float w, float h, rgba32 color);
void draw_circle(float x, float y, float r, rgba32 color);
void draw_obround(float x, float y, float w, float h, rgba32 color);
void draw_line(float x0, float y0, float x1, float y1, float width, rgba32 color);
void draw_line_round_cap(float x0, float y0, float x1, float y1, float width, rgba32 color);
void draw_arc(float x, float y, float r, float min_angel, float max_angle, float width, rgba32 color);
void stroke_rect(float x, float y, float w, float h, float width, rgba32 color);
void stroke_circle(float x, float y, float r, float width, rgba32 color);
void draw_image(float x, float y, float scale_factor, image img);
void draw_text(const char* text, float x, float y, float size, rgba32 c, font* font);

void draw_rect(rect r, rgba32 color) {
    draw_rect(r.min.x, r.min.y, r.max.x - r.min.x, r.max.y - r.min.y, color);
}

float v2_len_sqr(vec2 v) {
    return v.x * v.x + v.y * v.y;
}

bool point_in_rect(float x, float y, float min_x, float min_y, float max_x, float max_y) {
    return min_x < x && x < max_x && min_y < y && y < max_y;
}

bool point_in_rect(vec2 p, rect r) {
    return point_in_rect(p.x, p.y, r.min.x, r.min.y, r.max.x, r.max.y);
}

bool point_in_circle(vec2 point, vec2 center, float radius) {
    return v2_len_sqr(point - center) < radius * radius;
}

vec2 transform_point(vec2 p, mat3 t) {
    return (t * v3(p.x, p.y, 1)).xy;
}

vec2 to_screen_space(vec2 p) {
    return transform_point(p, get_transform());
}

float lerp(float a, float b, float t) {
    return (1 - t) * a + t * b;
}
vec2 lerp(vec2 a, vec2 b, float t) {
    return (1 - t) * a + t * b;
}

float length(vec2 v) {
    return hypotf(v.x, v.y);
}

//
// Game code
//

struct FPS_Counter {
    int fps;
    int frame_time_idx;
    float frame_times[10];
};

void update(FPS_Counter* c, float dt) {
    c->frame_times[c->frame_time_idx++] = dt;
    c->frame_time_idx %= ARRAY_LEN(c->frame_times);
    float acc = 0;
    for (size_t i = 0; i < ARRAY_LEN(c->frame_times); ++i) {
        acc += c->frame_times[i];
    }
    c->fps = 1.0 / (acc / ARRAY_LEN(c->frame_times));
}

static FPS_Counter fps_counter;

#include "stdarg.h"
static float text_y = 0;
void println(const char *fmt, ...) {
    float text_size = 20;
    va_list args;
    va_start(args, fmt);
    char text_buff[128];
    vsnprintf(text_buff, sizeof(text_buff), fmt, args);
    draw_text(text_buff, 0, text_y += text_size, text_size, Color(1, 1, 1), Font("msyh.ttc"));
    va_end(args);
}

enum GameScreen {
    SCREEN_Home,
    SCREEN_Explore,
    SCREEN_Compose,
    SCREEN_Collect,
};

const int WIDTH = 1200;
const int HEIGHT = 700;
GameScreen current_screen = SCREEN_Home;
bool mouse_was_down = false;
vec2 mouse_pos;
bool mouse_pressed;
bool mouse_released;
font* button_font;

bool draw_button(rect r, rgba32 color, const char* text) {
    if (point_in_rect(mouse_pos, r)) {
        draw_rect(r, color);
        if (mouse_pressed) {
            return true;
        }
    } else {
        color.r *= 0.8;
        color.g *= 0.8;
        color.b *= 0.8;
        draw_rect(r, color);
    }
    float h = r.max.y - r.min.y;
    draw_text(text, r.min.x, r.min.y + h*.6, h*.5, Color(1, 1, 1), button_font);
    return false;
}

void on_load() {
    button_font = Font("msyh.ttc");
}

void draw(float dt, int window_width, int window_height, game_inputs inputs) {
    update(&fps_counter, dt);
    text_y = 0;
    mouse_pos = v2(inputs.mouse_x, inputs.mouse_y);

    println("FPS: %d", fps_counter.fps);
    println("W: %d, H: %d", window_width, window_height);

    mouse_pressed = !mouse_was_down && inputs.mouse_down;
    mouse_released = mouse_was_down && !inputs.mouse_down;
    mouse_was_down = inputs.mouse_down;

    stroke_rect(0, 0, WIDTH, HEIGHT, 3, Color(1, 1, 1));

    if (current_screen == SCREEN_Home) {
        int button_width = WIDTH * 0.25;
        int button_height = HEIGHT * 0.25;

        rect button1 = RectWithPosAndSize(WIDTH * 0.6, HEIGHT / 6.0 - button_height * 0.5, button_width, button_height);
        if (draw_button(button1, Color(1, 0.2, 0.3), "知识探索")) {
            current_screen = SCREEN_Explore;
        }
        
        rect button2 = RectWithPosAndSize(WIDTH * 0.6, HEIGHT / 2.0 - button_height * 0.5, button_width, button_height);
        if (draw_button(button2, Color(.2, 0.8, 0.3), "合成探索")) {
            current_screen = SCREEN_Compose;
        }

        rect button3 = RectWithPosAndSize(WIDTH * 0.6, HEIGHT / 6.0 * 5.0 - button_height * 0.5, button_width, button_height);
        if (draw_button(button3, Color(1, .6, 0.2), "收集")) {
            current_screen = SCREEN_Collect;
        }

    } else if (current_screen == SCREEN_Explore) {
        println("探索 !!!!");

        static float x = 100;
        static float y = 100;
        static float offx;
        static float offy;
        
        static bool dragged = false;
        rect explain_button = RectWithPosAndSize(x, y, 50, 50);

        if (!dragged && point_in_rect(mouse_pos, explain_button) && mouse_pressed) {
            dragged = true;
            offx = mouse_pos.x - x;
            offy = mouse_pos.y - y;
        }

        if (dragged) {
            x = mouse_pos.x - offx;
            y = mouse_pos.y - offy;

            if (mouse_released) {
                dragged = false;
            }
        }

        static bool explain_opened = false;
        if (draw_button(explain_button, Color(.2, .3, .6), explain_opened ? "关闭" : "打开")) {
            explain_opened = !explain_opened;
        }
       
    } else if (current_screen == SCREEN_Compose) {
        println("合成 !!!!");
         if (mouse_pressed) {
            current_screen = SCREEN_Home;
        }
    } else if (current_screen == SCREEN_Collect) {
        println("收集 !!!!");
        if (mouse_pressed) {
            current_screen = SCREEN_Home;
        }
    }

}
