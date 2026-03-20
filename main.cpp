#include <string.h>
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include "app.hpp"
#include "log.hpp"

int main(int argc, char** argv)
{
    // parse -l before anything else
    int dst = 1;
    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            if(!Log::parse(argv[++i])) {
                fprintf(stderr, "bad -l spec: %s\n"
                    "usage: -l <level>  or  -l <comp>=<level>[,...]\n"
                    "levels: err wrn inf dbg dmp\n", argv[i]);
                return 1;
            }
        } else {
            argv[dst++] = argv[i];
        }
    }
    argc = dst;

    SDL_Init(SDL_INIT_VIDEO);

    App app;
    app.init(argc, argv);
    app.run();
    app.exit();

    SDL_Quit();
    return 0;
}

