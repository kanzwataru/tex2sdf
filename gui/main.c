#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#define GLAD_GLES2_IMPLEMENTATION
#include "extern/gles2.h"

struct GPU_Rect_Vertex
{
    float x, y;
    float u, v;
    float r, g, b;
};

struct App
{
    int window_width;
    int window_height;

    GLuint dummy_vao;
    GLuint vertex_buffer;

    GLuint shader_rect;

    struct GPU_Rect_Vertex ui_vertex_buffer[4096 * 6];
    uint32_t ui_vertex_buffer_top;
};

#define countof(x) (sizeof(x) / sizeof(x[0]))

const char *shader_rect_vert =
"#version 300 es\n"
"layout(location = 0) in vec2 vert_pos;\n"
"layout(location = 1) in vec2 vert_uv;\n"
"layout(location = 2) in vec3 vert_color;\n"
"\n"
"out vec3 frag_color;\n"
"out vec2 frag_uv;\n"
"\n"
"void main() {\n"
"   gl_Position = vec4(vert_pos, 0.0, 1.0);\n"
"   frag_color = vert_color;\n"
"   frag_uv = vert_uv;\n"
"}\n"
"";

const char *shader_rect_frag =
"#version 300 es\n"
"precision highp float;\n"
"\n"
"in vec3 frag_color;\n"
"in vec2 frag_uv;\n"
"\n"
"out vec4 out_color;\n"
"\n"
"void main() {\n"
"   out_color = vec4(frag_color, 1.0);\n"
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

static void draw_quad(struct App *ctx, float x1, float y1, float x2, float y2, float r, float g, float b)
{
    if(ctx->ui_vertex_buffer_top + 6 > countof(ctx->ui_vertex_buffer)) {
        return;
    }

    pixel_to_ndc(ctx, &x1, &y1);
    pixel_to_ndc(ctx, &x2, &y2);

    ctx->ui_vertex_buffer[ctx->ui_vertex_buffer_top++] = (struct GPU_Rect_Vertex){ .x = x2, .y = y2, .r = r, .g = g, .b = b };
    ctx->ui_vertex_buffer[ctx->ui_vertex_buffer_top++] = (struct GPU_Rect_Vertex){ .x = x1, .y = y2, .r = r, .g = g, .b = b };
    ctx->ui_vertex_buffer[ctx->ui_vertex_buffer_top++] = (struct GPU_Rect_Vertex){ .x = x1, .y = y1, .r = r, .g = g, .b = b };

    ctx->ui_vertex_buffer[ctx->ui_vertex_buffer_top++] = (struct GPU_Rect_Vertex){ .x = x2, .y = y2, .r = r, .g = g, .b = b };
    ctx->ui_vertex_buffer[ctx->ui_vertex_buffer_top++] = (struct GPU_Rect_Vertex){ .x = x2, .y = y1, .r = r, .g = g, .b = b };
    ctx->ui_vertex_buffer[ctx->ui_vertex_buffer_top++] = (struct GPU_Rect_Vertex){ .x = x1, .y = y1, .r = r, .g = g, .b = b };
}

void app_init(struct App *ctx)
{
	glGenVertexArrays(1, &ctx->dummy_vao);
	glBindVertexArray(ctx->dummy_vao);

    ctx->shader_rect = gl_compile_shader_program(shader_rect_vert, shader_rect_frag);
}

void app_update_and_render(struct App *ctx)
{
    glClearColor(0.7f, 0.9f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ctx->ui_vertex_buffer_top = 0;

    draw_quad(ctx, 32, 32, 64, 64, 1.0f, 0.5f, 0.25f);

    GLuint vertex_buffer;
    glGenBuffers(1, &vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ctx->ui_vertex_buffer[0]) * ctx->ui_vertex_buffer_top, ctx->ui_vertex_buffer, GL_STATIC_DRAW);

    glUseProgram(ctx->shader_rect);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, false, sizeof(struct GPU_Rect_Vertex), 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(struct GPU_Rect_Vertex), (void *)(sizeof(float) * 2));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, false, sizeof(struct GPU_Rect_Vertex), (void *)(sizeof(float) * 4));

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
