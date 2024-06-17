#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#define GLAD_GLES2_IMPLEMENTATION
#include "extern/gles2.h"

#define STB_IMAGE_IMPLEMENTATION
#include "extern/stb_image.h"

#include "font/droid_sans.h"

struct Rect
{
    float x1, y1;
    float x2, y2;
};

struct Color
{
    float r, g, b, a;
};

enum
{
    UI_RECT_FLAG_IS_TEXT = 1 << 0
};

struct UI_Rect
{
    float x1, y1;
    float x2, y2;

    float u1, v1;
    float u2, v2;

    float r, g, b, a;

    uint32_t flags;
};

struct GPU_Rect_Vertex
{
    float x, y;
    float u, v, flags;
    float r, g, b, a;
};

enum
{
    UI_WIDGET_FLAG_VISIBLE = 1 << 0,
    UI_WIDGET_FLAG_CLICKABLE = 1 << 1
};

struct UI_Widget
{
    struct UI_Widget *next;
    struct UI_Widget *prev;
    struct UI_Widget *parent;
    struct UI_Widget *child;

    struct Rect rect;
    struct Color color;
    const char *text;

    uint32_t flags;
    uint64_t id;
};

struct UI
{
    struct UI_Widget widget_pool[1024];
    uint32_t widget_pool_top;

    struct UI_Widget *widget_first;
    struct UI_Widget *widget_last;

    float cursor_x;
    float cursor_y;

    uint64_t hovering_widget_id;
};

struct App
{
    // -- TODO: Split this stuff into a Draw context struct
    int window_width;
    int window_height;

    GLuint dummy_vao;
    GLuint vertex_buffer;

    GLuint shader_rect;

    struct GPU_Rect_Vertex ui_vertex_buffer[4096 * 6];
    uint32_t ui_vertex_buffer_top;

    const struct Font *font;
    GLuint texture_font;
    // --

    struct UI ui;
};

#define countof(x) (sizeof(x) / sizeof(x[0]))

const char *shader_rect_vert =
"#version 300 es\n"
"precision highp float;\n"
"layout(location = 0) in vec2 vert_pos;\n"
"layout(location = 1) in vec3 vert_uv;\n"
"layout(location = 2) in vec4 vert_color;\n"
"\n"
"out vec4 frag_color;\n"
"out vec2 frag_uv;\n"
"flat out int frag_flags;\n"
"\n"
"void main() {\n"
"   gl_Position = vec4(vert_pos, 0.0, 1.0);\n"
"   frag_color = vert_color;\n"
"   frag_uv = vert_uv.xy;\n"
"   frag_flags = floatBitsToInt(vert_uv.z);\n"
"}\n"
"";

const char *shader_rect_frag =
"#version 300 es\n"
"precision highp float;\n"
"\n"
"in vec4 frag_color;\n"
"in vec2 frag_uv;\n"
"flat in int frag_flags;\n"
"\n"
"out vec4 out_color;\n"
"\n"
"uniform sampler2D tex_fontsheet;\n"
"\n"
"void main() {\n"
"   vec4 color = frag_color;\n"
"\n"
"   if((frag_flags & 1) != 0) {\n"
"       float sdf = texture(tex_fontsheet, frag_uv).r;\n"
"       color.a *= step(0.5, sdf);\n"
"   }\n"
"\n"
"   out_color = color;\n"
"}\n"
"";

static void panic(const char *error)
{
    puts(error);
    exit(1);
}

static GLuint gl_compile_shader(GLenum type, const char *src)
{
    GLuint handle = glCreateShader(type);
    glShaderSource(handle, 1, &src, NULL);
    glCompileShader(handle);

    GLint success;
    glGetShaderiv(handle, GL_COMPILE_STATUS, &success);
    if(!success) {
        char err[1024];
        glGetShaderInfoLog(handle, sizeof(err), NULL, err);
        panic(err);
    }

    return handle;
}

static GLuint gl_compile_shader_program(const char *vert, const char *frag)
{
    GLuint vert_shader = gl_compile_shader(GL_VERTEX_SHADER, vert);
    GLuint frag_shader = gl_compile_shader(GL_FRAGMENT_SHADER, frag);

    GLuint handle = glCreateProgram();
    glAttachShader(handle, vert_shader);
    glAttachShader(handle, frag_shader);
    glLinkProgram(handle);

    GLint success;
    glad_glGetProgramiv(handle, GL_LINK_STATUS, &success);
    if(!success) {
        char err[1024];
        glGetProgramInfoLog(handle, sizeof(err), NULL, err);
        panic(err);
    }

    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);

    return handle;
}

static void pixel_to_ndc(struct App *ctx, float *x, float *y)
{
    *x = (*x / ctx->window_width) * 2.0f - 1.0f;
    *y = (1.0f - *y / ctx->window_height) * 2.0f - 1.0f;
}

static void draw_rect(struct App *ctx, struct UI_Rect r)
{
    if(ctx->ui_vertex_buffer_top + 6 > countof(ctx->ui_vertex_buffer)) {
        return;
    }

    pixel_to_ndc(ctx, &r.x1, &r.y1);
    pixel_to_ndc(ctx, &r.x2, &r.y2);

    const float flags_float = *(float *)&r.flags;

    ctx->ui_vertex_buffer[ctx->ui_vertex_buffer_top++] = (struct GPU_Rect_Vertex){ .x = r.x2, .y = r.y2, .r = r.r, .g = r.g, .b = r.b, .a = r.a, .u = r.u2, .v = r.v2, .flags = flags_float };
    ctx->ui_vertex_buffer[ctx->ui_vertex_buffer_top++] = (struct GPU_Rect_Vertex){ .x = r.x1, .y = r.y2, .r = r.r, .g = r.g, .b = r.b, .a = r.a, .u = r.u1, .v = r.v2, .flags = flags_float };
    ctx->ui_vertex_buffer[ctx->ui_vertex_buffer_top++] = (struct GPU_Rect_Vertex){ .x = r.x1, .y = r.y1, .r = r.r, .g = r.g, .b = r.b, .a = r.a, .u = r.u1, .v = r.v1, .flags = flags_float };

    ctx->ui_vertex_buffer[ctx->ui_vertex_buffer_top++] = (struct GPU_Rect_Vertex){ .x = r.x2, .y = r.y2, .r = r.r, .g = r.g, .b = r.b, .a = r.a, .u = r.u2, .v = r.v2, .flags = flags_float };
    ctx->ui_vertex_buffer[ctx->ui_vertex_buffer_top++] = (struct GPU_Rect_Vertex){ .x = r.x2, .y = r.y1, .r = r.r, .g = r.g, .b = r.b, .a = r.a, .u = r.u2, .v = r.v1, .flags = flags_float };
    ctx->ui_vertex_buffer[ctx->ui_vertex_buffer_top++] = (struct GPU_Rect_Vertex){ .x = r.x1, .y = r.y1, .r = r.r, .g = r.g, .b = r.b, .a = r.a, .u = r.u1, .v = r.v1, .flags = flags_float };
}

static void draw_text(struct App *ctx, const char *text, struct Rect bounds, struct Color color, float text_size)
{
    float x = bounds.x1;
    float y = bounds.y1;

    const float size_mult = text_size / (float)ctx->font->size;

    for(const char *c = text; *c; ++c)
    {
        if(*c == ' ')
        {
            x += ctx->font->characters[0].width * size_mult;
            continue;
        }
        else if(*c == '\n')
        {
            y += ctx->font->characters[0].height * size_mult;
            continue;
        }

        const int char_index = *c > 32 && *c < 127 ? *c - 32 : '?' - 32;
        const struct Character *font_char = &ctx->font->characters[char_index];

        const float x1 = x - font_char->origin_x * size_mult;
        const float y1 = y - font_char->origin_y * size_mult;

        draw_rect(ctx, (struct UI_Rect){
            .x1 = x1, .y1 = y1,
            .x2 = x1 + font_char->width * size_mult, .y2 = y1 + font_char->height * size_mult,

            .u1 = (float)font_char->x / ctx->font->width,
            .v1 = (float)font_char->y / ctx->font->height,
            .u2 = (float)(font_char->x + font_char->width) / ctx->font->width,
            .v2 = (float)(font_char->y + font_char->height) / ctx->font->height,

            .r = color.r, .g = color.g, .b = color.b, .a = color.a,

            .flags = UI_RECT_FLAG_IS_TEXT
        });

        x += font_char->advance * size_mult;

        // TODO: Line wrapping
    }
}

static void ui_begin(struct UI *ui)
{
    // Re-initialize layout
    ui->cursor_x = 128.0f;
    ui->cursor_y = 32.0f;

    // Process interaction
    // TODO: Implement...

    // Clear widget list
    ui->widget_pool_top = 1;
    ui->widget_pool[0] = (struct UI_Widget){};
    ui->widget_first = &ui->widget_pool[0];
    ui->widget_last = &ui->widget_pool[0];
}

static void ui_end(struct UI *ui, struct App *ctx)
{
    for(struct UI_Widget *w = ui->widget_first; w; w = w->next) {
        if(!(w->flags & UI_WIDGET_FLAG_VISIBLE)) {
            continue;
        }

        if(w->text) {
            draw_text(ctx, w->text, w->rect, w->color, 32.0f);
        }
        else {
            draw_rect(ctx, (struct UI_Rect){
                .x1 = w->rect.x1, .y1 = w->rect.y1,
                .x2 = w->rect.x2, .y2 = w->rect.y2,

                .r = w->color.r, .g = w->color.g, .b = w->color.b, .a = w->color.a,
            });
        }
    }
}

static inline void ui_push_widget(struct UI *ui, struct UI_Widget *in_widget)
{
    if(ui->widget_pool_top + 1 < countof(ui->widget_pool)) {
        struct UI_Widget *widget = &ui->widget_pool[ui->widget_pool_top++];
        *widget = *in_widget;

        if(!widget->parent) {
            ui->widget_last->next = widget;
            widget->prev = ui->widget_last;
            ui->widget_last = widget;
        }
    }
}

static bool ui_button(struct UI *ui, const char *label)
{
    ui_push_widget(ui, &(struct UI_Widget){
        .rect = { ui->cursor_x, ui->cursor_y, ui->cursor_x + 128.0f, ui->cursor_y + 32.0f },
        .color = { 0.5f, 0.5f, 0.5f, 1.0f },
        .flags = UI_WIDGET_FLAG_VISIBLE | UI_WIDGET_FLAG_CLICKABLE,
        .id = (uint64_t)label
    });

    ui_push_widget(ui, &(struct UI_Widget){
        .rect = { ui->cursor_x + 16.0f, ui->cursor_y + 16.0f, ui->cursor_x + 128.0f, ui->cursor_y + 32.0f },
        .color = { 0.01f, 0.01f, 0.01f, 1.0f },
        .text = label,
        .flags = UI_WIDGET_FLAG_VISIBLE
        //.parent =  // TODO: Add hierarchy
        //.id = (uint64_t)label // TODO: How should child widgets get IDs???
    });

    return false; // TODO: Interaction
}

static void app_init(struct App *ctx)
{
	glGenVertexArrays(1, &ctx->dummy_vao);
	glBindVertexArray(ctx->dummy_vao);

    ctx->shader_rect = gl_compile_shader_program(shader_rect_vert, shader_rect_frag);

    {
        ctx->font = &font_droid_sans;

        //stbi_set_flip_vertically_on_load(true);

        int width, height, channels_in_file;
        uint8_t *buffer = stbi_load_from_memory(ctx->font->png_data, ctx->font->png_data_size, &width, &height, &channels_in_file, 1);

        glGenTextures(1, &ctx->texture_font);
        glBindTexture(GL_TEXTURE_2D, ctx->texture_font);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, buffer);
    }
}

static void app_update_and_render(struct App *ctx)
{
    ctx->ui_vertex_buffer_top = 0;

    // * Update
    ui_begin(&ctx->ui);

    ui_button(&ctx->ui, "Button!");

    ui_end(&ctx->ui, ctx);

    draw_rect(ctx, (struct UI_Rect){
        .x1 = 32, .y1 = 32,
        .x2 = 64, .y2 = 64,
        .r = 0.0f, .g = 1.0f, .b = 1.0f, .a = 1.0f
    });

    draw_text(ctx, "Hello, world!", (struct Rect){ 128.0f, 128.0f }, (struct Color){ 1.0f, 1.0f, 0.0f, 1.0f }, 32.0f);
    draw_text(ctx, "Hello, world!", (struct Rect){ 192.0f, 256.0f }, (struct Color){ 0.0f, 1.0f, 1.0f, 1.0f }, 64.0f);
    draw_text(ctx, "Hello, world!", (struct Rect){ 64.0f, 350.0f }, (struct Color){ 1.0f, 0.0f, 0.0f, 1.0f }, 16.0f);

    // * Render
    glClearColor(0.7f, 0.9f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    GLuint vertex_buffer;
    glGenBuffers(1, &vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ctx->ui_vertex_buffer[0]) * ctx->ui_vertex_buffer_top, ctx->ui_vertex_buffer, GL_STATIC_DRAW);

    glUseProgram(ctx->shader_rect);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, false, sizeof(struct GPU_Rect_Vertex), 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, false, sizeof(struct GPU_Rect_Vertex), (void *)(sizeof(float) * 2));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, false, sizeof(struct GPU_Rect_Vertex), (void *)(sizeof(float) * 5));

    glBindTexture(GL_TEXTURE_2D, ctx->texture_font);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDrawArrays(GL_TRIANGLES, 0, ctx->ui_vertex_buffer_top);

    glDeleteBuffers(1, &vertex_buffer);
}

int main(int argc, char **argv)
{
    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    static struct App ctx = {0};
    ctx.window_width = 800;
    ctx.window_height = 600;

    SDL_Window *window = SDL_CreateWindow("tex2sdf", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ctx.window_width, ctx.window_height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    gladLoadGLES2((GLADloadfunc)SDL_GL_GetProcAddress);

    app_init(&ctx);

    bool running = true;
    while(running) {
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            switch(event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
            }
        }

        app_update_and_render(&ctx);

        SDL_GL_SwapWindow(window);
    }

    return 0;
}
