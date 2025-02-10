// link line: -lgdi32 -lopengl32

#include "stdbool.h"

typedef enum {
    Event_close,
    Event_key_up,
    Event_key_down,
    Event_mouse_button_up,
    Event_mouse_button_down,
} Event_Type;

typedef enum {
    Button_left,
} Mouse_Button;

typedef struct {
    Event_Type type;
    union {
        char key; // Event_key_{up/down}, vk code: https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
        Mouse_Button button; // Event_mouse_button_{up/down}
    };
} Event;

typedef struct Window Window;

Window* create_window();
void destroy_window(const Window* win);
bool window_check_event(Window* win, Event* event);
bool window_should_close(Window* win);
void window_make_current(const Window* win);
void window_swap_buffers(const Window* win);
// Load OpenGL functions into the address space of the current process,
// a gl context should have been bound to the current thread with wglMakeCurrent()
// before using this function.
void load_gl_procs();

//
// Implementation
//

#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include "GL/gl.h"
#include "GL/glext.h"
#include "assert.h"

#define ARRAY_LEN(a) (sizeof(a)/sizeof*(a))
#define Barrier() asm volatile("" ::: "memory")

#define GL_PROCS \
XXX(PFNGLCREATESHADERPROC,              glCreateShader) \
XXX(PFNGLSHADERSOURCEPROC,              glShaderSource) \
XXX(PFNGLCOMPILESHADERPROC,             glCompileShader) \
XXX(PFNGLGETSHADERIVPROC,               glGetShaderiv) \
XXX(PFNGLGETSHADERINFOLOGPROC,          glGetShaderInfoLog) \
XXX(PFNGLDELETESHADERPROC,              glDeleteShader) \
XXX(PFNGLCREATEPROGRAMPROC,             glCreateProgram) \
XXX(PFNGLATTACHSHADERPROC,              glAttachShader) \
XXX(PFNGLLINKPROGRAMPROC,               glLinkProgram) \
XXX(PFNGLGETPROGRAMIVPROC,              glGetProgramiv) \
XXX(PFNGLGETPROGRAMINFOLOGPROC,         glGetProgramInfoLog) \
XXX(PFNGLDELETEPROGRAMPROC,             glDeleteProgram) \
XXX(PFNGLUSEPROGRAMPROC,                glUseProgram) \
XXX(PFNGLGETUNIFORMLOCATIONPROC,        glGetUniformLocation) \
XXX(PFNGLUNIFORM1IPROC,                 glUniform1i) \
XXX(PFNGLUNIFORM2FPROC,                 glUniform2f) \
XXX(PFNGLUNIFORM4FPROC,                 glUniform4f) \
XXX(PFNGLUNIFORMMATRIX2FVPROC,          glUniformMatrix2fv) \
XXX(PFNGLUNIFORMMATRIX3FVPROC,          glUniformMatrix3fv) \
XXX(PFNGLACTIVETEXTUREPROC,             glActiveTexture) \
XXX(PFNGLGENBUFFERSPROC,                glGenBuffers) \
XXX(PFNGLBINDBUFFERPROC,                glBindBuffer) \
XXX(PFNGLBUFFERDATAPROC,                glBufferData) \
XXX(PFNGLGETATTRIBLOCATIONPROC,         glGetAttribLocation) \
XXX(PFNGLENABLEVERTEXATTRIBARRAYPROC,   glEnableVertexAttribArray) \
XXX(PFNGLVERTEXATTRIBPOINTERPROC,       glVertexAttribPointer) \
XXX(PFNGLVERTEXATTRIBIPOINTERPROC,      glVertexAttribIPointer) \
XXX(PFNGLVERTEXATTRIBDIVISORPROC,       glVertexAttribDivisor) \
XXX(PFNGLDRAWARRAYSINSTANCEDPROC,       glDrawArraysInstanced) \
// GL_PROCS

#define XXX(type, name) type name = NULL;
GL_PROCS
#undef XXX

void load_gl_procs() {
#define XXX(type, name) assert(name = (type)(void*)wglGetProcAddress(#name));
GL_PROCS
#undef XXX
}

static DWORD create_window_thread_id = 0;

struct Window {
    unsigned int width, height;
    int mouse_x, mouse_y;

    // internal
    HWND hwnd;
    HDC hdc;
    HGLRC hglrc;

    int event_read_index;
    int event_write_index;
    Event events[100];

    bool should_close;
};

Window* thread_create_window(const WNDCLASSA* opengl_window_class) {
    Window* win = (Window*)malloc(sizeof(*win));
    assert(win);
    memset(win, 0, sizeof(*win));

    // Pass the Window struct through the create param to store it on the
    // window handle when it's first created. This is to make sure the pointer
    // is valid when the window proc is called.
    HWND hwnd = CreateWindowA(opengl_window_class->lpszClassName, "OpenGL",
                              WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                              CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                              0, 0, 0, win);
    assert(hwnd);

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;

    HDC hdc = GetDC(hwnd);
    int pixel_format = ChoosePixelFormat(hdc, &pfd);
    assert(pixel_format);
    SetPixelFormat(hdc, pixel_format, &pfd);
    HGLRC hglrc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, hglrc);

    win->hwnd = hwnd;
    win->hdc = hdc;
    win->hglrc = hglrc;

    return win;
}

void window_push_event(Window* win, Event event) {
    int new_write_index = (win->event_write_index + 1) % ARRAY_LEN(win->events);
    assert(new_write_index != win->event_read_index);
    win->events[win->event_write_index] = event;
    Barrier();
    win->event_write_index = new_write_index;
}

bool window_check_event(Window* win, Event* event) {
    if (win->event_read_index == win->event_write_index) return false; // no event
    *event = win->events[win->event_read_index];
    Barrier();
    win->event_read_index = (win->event_read_index + 1) % ARRAY_LEN(win->events);
    if (event->type == Event_close) win->should_close = true;
    return true;
}

bool window_should_close(Window* win) {
    if (win->should_close) {
        win->should_close = false;
        return true;
    }
    return false;
}

LRESULT window_proc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    // Get the pointer to the Window struct from hWnd
    Window* win = (Window*)GetWindowLongPtrA(hWnd, GWLP_USERDATA);
    switch (Msg) {
        case WM_CREATE: {
            // Store a pointer back to the Window structure to access the data
            // inside the window procedure. This is passed in via CreateWindow().
            CREATESTRUCTA* create_struct = (CREATESTRUCTA*)lParam;
            SetWindowLongPtrA(hWnd, GWLP_USERDATA, (LONG_PTR)create_struct->lpCreateParams);
        } break;

        case WM_CLOSE: {
            Event ev;
            ev.type = Event_close;
            window_push_event(win, ev);
        } break;

        case WM_SIZE: {
            win->width = LOWORD(lParam);
            win->height = HIWORD(lParam);
        } break;

        default: return DefWindowProc(hWnd, Msg, wParam, lParam);
    }
    return 0;
}

struct Thread_Create_Window_Params {
    DWORD thread_id; // The thread that requested to create a window
};
#define THREAD_CREATE_WINDOW (WM_USER+100)
#define THREAD_DESTROY_WINDOW (WM_USER+101)
#define THREAD_WINDOW_CREATED (WM_USER+102)

DWORD create_window_thread_proc(LPVOID lpParameter) {
    WNDCLASSA wc = {};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = window_proc;
    wc.hCursor = LoadCursorA(0, IDC_ARROW);
    wc.lpszClassName = "OpenGL Window Class";
    assert(RegisterClassA(&wc));

    struct Thread_Create_Window_Params* p = (struct Thread_Create_Window_Params*)lpParameter;

    // The thread starts when the first time create_window() gets called,
    // so we create a window first. If this succeeds, that means we have
    // initilized an OpenGL context, so we can load GL functions afterwards.
    Window* win = thread_create_window(&wc);
    assert(win);
    load_gl_procs();
    wglMakeCurrent(win->hdc, 0);
    // Signal the other thread that the window has been created.
    PostThreadMessageA(p->thread_id, THREAD_WINDOW_CREATED, (WPARAM)win, 0);

    MSG msg;
    while (GetMessageA(&msg, 0, 0, 0) >= 0) {
        if (msg.message ==THREAD_CREATE_WINDOW) {
            // New call of create_window()
            // wParam stores the paramerters for the call
            struct Thread_Create_Window_Params* p = (struct Thread_Create_Window_Params*)msg.wParam;
            Window* win = thread_create_window(&wc);
            assert(win);
            wglMakeCurrent(win->hdc, 0);
            PostThreadMessageA(p->thread_id, THREAD_WINDOW_CREATED, (WPARAM)win, 0);
        } else if (msg.message == THREAD_DESTROY_WINDOW) {
            Window* win = (Window*)msg.wParam;
            DestroyWindow(win->hwnd);
            free(win);
        } else {
            if (msg.hwnd) {
                // handle window messages
                Window* win = (Window*)GetWindowLongPtrA(msg.hwnd, GWLP_USERDATA);
                switch (msg.message) {
                    case WM_KEYUP: {
                        Event ev;
                        ev.type = Event_key_up;
                        ev.key = msg.wParam;
                        window_push_event(win, ev);
                    } break;

                    case WM_KEYDOWN: {
                        Event ev;
                        ev.type = Event_key_down;
                        ev.key = msg.wParam;
                        window_push_event(win, ev);
                    } break;

                    case WM_MOUSEMOVE: {
                        POINTS p = MAKEPOINTS(msg.lParam);
                        win->mouse_x = p.x;
                        win->mouse_y = p.y;
                    } break;

                    case WM_LBUTTONUP: {
                        Event ev;
                        ev.type = Event_mouse_button_up;
                        ev.button = Button_left;
                        window_push_event(win, ev);
                    } break;

                    case WM_LBUTTONDOWN: {
                        Event ev;
                        ev.type = Event_mouse_button_down;
                        ev.button = Button_left;
                        window_push_event(win, ev);
                    } break;
                }
            }

            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    assert(false); // GetMessage() Errored

    return 0;
}

Window* create_window() {
    struct Thread_Create_Window_Params params = {};
    params.thread_id = GetCurrentThreadId();

    if (!create_window_thread_id) {
        HANDLE create_window_thread = CreateThread(0, 0, create_window_thread_proc, &params, 0, &create_window_thread_id);
        assert(create_window_thread);
    } else {
        PostThreadMessageA(create_window_thread_id, THREAD_CREATE_WINDOW, (WPARAM)&params, 0);
    }

    // Wait for the THREAD_WINDOW_CREATED message
    MSG msg;
    GetMessageA(&msg, 0, THREAD_WINDOW_CREATED, THREAD_WINDOW_CREATED);
    Window* win = (Window*)msg.wParam;
    wglMakeCurrent(win->hdc, win->hglrc);
    return win;
}

void destroy_window(const Window* win) {
    assert(create_window_thread_id);
    PostThreadMessageA(create_window_thread_id, THREAD_DESTROY_WINDOW, (WPARAM)win, 0);
}

void window_make_current(const Window* win) {
    wglMakeCurrent(win->hdc, win->hglrc);
}

void window_swap_buffers(const Window* win) {
    SwapBuffers(win->hdc);
}
