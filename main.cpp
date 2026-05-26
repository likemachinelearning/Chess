#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <cmath>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <pdh.h>
#pragma comment(lib, "pdh.lib")
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <SDL3_image/SDL_image.h>

#include "GuiRenderer.hpp"

// Globals
AssetManager assets;
GameContext context;
GuiRenderer gui;

const int WINDOW_WIDTH = 1280;
const int WINDOW_HEIGHT = 720;

int main(int argc, char** argv) {
#ifdef _WIN32
    FreeConsole();
#endif
    SDL_SetMainReady();
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_CRITICAL);
    SDL_SetAppMetadata("Chess Application", "1.0", "com.example.chess");

    if (!SDL_Init(0)) return -1;
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) return -1;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("Warning: Audio Init Failed");
    }
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {}

    const char* glsl_version = "#version 330";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    SDL_Window* window = SDL_CreateWindow("Chess Application", WINDOW_WIDTH, WINDOW_HEIGHT, (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY));
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); 
    SDL_ShowWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Initialize Systems
    assets.Init(window);
    context.Init(&assets);
    gui.Init(&context, &assets, window);
    
    context.game.reset();
    assets.PlaySound(assets.snd_startup);

    const int TARGET_FPS = 60;
    const int FRAME_DELAY = 1000 / TARGET_FPS;

    bool done = false;
    while (!done) {
        Uint64 frame_start = SDL_GetTicks();
        SDL_Event event;
        bool continuous = context.NeedsContinuousUpdate();

        if (continuous) {
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL3_ProcessEvent(&event);
                if (event.type == SDL_EVENT_QUIT) {
                    gui.SaveSettings();
                    done = true;
                }
                if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window)) {
                    gui.SaveSettings();
                    done = true;
                }
            }
        } else {
            int idle_timeout = context.GetIdleTimeout();
            if (SDL_WaitEventTimeout(&event, idle_timeout)) {
                context.idle_activity_timer = 0.0f; // Reset on event
                do {
                    ImGui_ImplSDL3_ProcessEvent(&event);
                    if (event.type == SDL_EVENT_QUIT) {
                        gui.SaveSettings();
                        done = true;
                    }
                    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window)) {
                        gui.SaveSettings();
                        done = true;
                    }
                } while (SDL_PollEvent(&event));
            } else {
                // Timeout reached, increment idle timer
                context.idle_activity_timer += (float)idle_timeout / 1000.0f;
            }
        }

        // Additional mouse delta check for reset
        if (ImGui::GetIO().MouseDelta.x != 0 || ImGui::GetIO().MouseDelta.y != 0) {
            context.idle_activity_timer = 0.0f;
        }

        float dt = ImGui::GetIO().DeltaTime;
        context.Update(dt);
        gui.Render();

        Uint64 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < (Uint64)FRAME_DELAY) {
            SDL_Delay(FRAME_DELAY - (Uint32)frame_time);
        }
    }

    // Cleanup
    assets.Cleanup();
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
