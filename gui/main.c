#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#define GLAD_GLES2_IMPLEMENTATION
#include "extern/gles2.h"

struct App
{
    int window_width;
    int window_height;
};

void app_init(struct App *ctx)
{

}

void app_update_and_render(struct App *ctx)
{
    glClearColor(0.7f, 0.9f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
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
