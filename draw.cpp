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

struct Draggable {
    vec2 pos;
    vec2 mouse_off;
    bool dragged;
};

enum EntityType {
    ENTITY_BALL,
    ENTITY_BUTTON,
};

struct Entity {
    EntityType type;
    const char* text;
    bool active; // 用来控制是否显示这个实体

    // ball
    float radius;
    Draggable drag;
    bool selected; // 当前球是否被选中

    // button
    rect r;
    rgba32 color;
};

Draggable make_draggable(float x, float y) {
    Draggable d;
    d.pos = v2(x, y);
    d.mouse_off = v2(0, 0);
    d.dragged = false;
    return d;
}

const int WIDTH = 1200;
const int HEIGHT = 700;
GameScreen current_screen = SCREEN_Compose;
bool mouse_was_down = false;
vec2 mouse_pos;
bool mouse_pressed;
bool mouse_released;
font* button_font;
Entity entities[1000];
int entity_count = 0;

void add_button_entity(rect r, rgba32 color, const char* text) {
    if (entity_count >= (int)ARRAY_LEN(entities)) {
        printf("Error: Max entity count!!!!\n");
        return;
    }

    Entity* e = &entities[entity_count];
    entity_count += 1;

    e->type = ENTITY_BUTTON;
    e->active = true;
    e->r = r;
    e->color = color;
    e->text = text;
}

void add_ball_entity(float x, float y, float r, const char* text) {
    if (entity_count >= (int)ARRAY_LEN(entities)) {
        printf("Error: Max entity count!!!!\n");
        return;
    }

    Entity* e = &entities[entity_count];
    entity_count += 1;

    e->type = ENTITY_BALL;
    e->active = true;
    e->radius = r;
    e->selected = false;
    e->drag = make_draggable(x, y);
    e->text = text;
}

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

void draw_ball(vec2 pos, float r, const char* text, rgba32 color) {
    draw_circle(pos.x, pos.y, r, color);
    draw_text(text, pos.x, pos.y, r*2, Color(1, 1, 1), button_font);
}

void draggable_ball(Draggable* d, float r, const char* text, rgba32 color) {
    if (!d->dragged && point_in_circle(mouse_pos, d->pos, r) && mouse_pressed) {
        d->dragged = true;
        d->mouse_off = mouse_pos - d->pos;
    }
    if (d->dragged) {
        d->pos = mouse_pos - d->mouse_off;
        if (mouse_released) {
            d->dragged = false;
        }
    }
    draw_ball(d->pos, r, text, color);
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

    } else if (current_screen == SCREEN_Compose) {
        println("合成 !!!!");

        static bool is_selecting = false; // 使用这个变量来控制现在是否在选择球

        // 根据状态在左上角显示不同的文字
        if (is_selecting) {
            println("点击球以选择");
            println("三个abc球可以合成一个D球");
        } else {
            println("按住球可拖动");
            println("点击合成按钮可以选择球进行合成");
        }

        // 画一个可以拖出新球的球
        float x = 100;
        float y = 400;
        float r = 20;
        draw_circle(x, y, r, Color(1, 0, 0));
        if (point_in_circle(mouse_pos, v2(x, y), r) && mouse_pressed) {
            add_ball_entity(x, y, r, "abc"); // 添加一个abc球
        }

        // 画删除按钮
        if (draw_button(RectWithPosAndSize(500, 100, 200, 100), Color(0.2, 0.1, 0.7), "delete")) {
            entity_count = 0;
        }

        // 画用来控制是否合成的按钮
        rect selecting_button = RectWithPosAndSize(200, 400, 100, 50);
        rgba32 selecting_button_color = Color(0.2, 0.6, 0.3);
        const char* selecting_button_text = is_selecting ?  "取消合成" : "合成";
        // 按钮按下时：
        if (draw_button(selecting_button, selecting_button_color, selecting_button_text)) {
            is_selecting = !is_selecting; // 切换合成状态
        }

        Entity* selected_balls[10]; // 使用这个数组记录当前被选择的球
        int selected_ball_count = 0; // 当前选择了的球数量

        // 处理所有的实体
        for (int i = 0; i < entity_count; ++i) {
            Entity* e = &entities[i];
            if (!e->active) continue; // 跳过非active的实体

            if (e->type == ENTITY_BALL) {
                rgba32 ball_color = Color(1, 0, 0);
                if (is_selecting) {
                    // 当在选择时：
                    if (point_in_circle(mouse_pos, e->drag.pos, e->radius)) {
                        // 鼠标按下时设置它的selected状态
                        if (mouse_pressed) {
                            e->selected = !e->selected;
                        }
                    }
                    if (e->selected) {
                        // 如果当前被选中
                        ball_color = Color(0.8, 0.6, 0.2); // 改变颜色
                        selected_balls[selected_ball_count] = e;
                        selected_ball_count += 1;

                        if (selected_ball_count == 3) {
                            // 如果选中了3个球
                            for (int i = 0; i < selected_ball_count; ++i) {
                                selected_balls[i]->active = false; // 删除被选中的球
                            }
                            selected_ball_count = 0;
                            add_ball_entity(e->drag.pos.x, e->drag.pos.y, e->radius, "D"); // 在当前球的位置生成一个D球
                        }
                    }

                    // 画球
                    draw_ball(e->drag.pos, e->radius, e->text, ball_color);
                } else {
                    // 如果不在选择：
                    e->selected = false; // 清除球的选择状态
                    draggable_ball(&e->drag, e->radius, e->text, ball_color); // 显示拖动球
                }
            } else if (e->type == ENTITY_BUTTON) {
                draw_button(e->r, e->color, e->text);
            }
        }

    } else if (current_screen == SCREEN_Collect) {
        println("收集 !!!!");
        if (mouse_pressed) {
            current_screen = SCREEN_Home;
        }
    }

}
