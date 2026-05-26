#pragma once
#include "GameContext.hpp"
#include "AssetManager.hpp"
#include "SettingsManager.hpp"
#include "StatsManager.hpp"
#include "BoardGeometry.hpp"
#include "ThemeLibrary.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <ctime>
#include <fstream>

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
#include <shellapi.h>
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "shell32.lib")
#endif

class GuiRenderer {
public:
    GameContext* context;
    AssetManager* assets;
    SDL_Window* window;

    SettingsManager settings;
    ImFont* font_main = nullptr;
    ImFont* font_large = nullptr;

    bool theme_dirty = false;
    float BOARD_SIZE = 600.0f;
    float SQUARE_SIZE = 600.0f / 8.0f;
    bool show_title_bars = true;

    float cached_sin = 0.0f;
    float cached_cos = 1.0f;
    float last_flip_val = -1.0f;
    std::string keyboard_buffer = "";

    chess::Board cached_display_board;
    chess::Movelist cached_legal_moves;
    int last_cached_history_idx = -1;
    std::string last_cached_start_fen = "";
    
    ImVec2 last_board_pos = {0,0};
    float last_board_size = 600.0f;

    int last_combo_count = 0;
    float combo_pop_timer = 0.0f;
    float combo_inactivity_timer = 0.0f;

    bool show_help = false;
    bool reset_layout = false;

    // History Drill-down
    std::string drilldown_date = "";
    bool show_drilldown_modal = false;

    struct Star {
        float x, y;
        float speed;
        float size;
        float brightness;
        float target_brightness;
    };
    std::vector<Star> deep_space_stars;

    struct ShootingStar {
        bool active;
        float x, y;
        float vel_x, vel_y;
        float life;
    };
    ShootingStar shooting_star;

    struct CursorTrailParticle {
        float x, y;
        float size;
        float life;
    };
    std::vector<CursorTrailParticle> cursor_trail;

    GuiRenderer() = default;

    void Init(GameContext* ctx, AssetManager* ast, SDL_Window* win) {
        context = ctx;
        assets = ast;
        window = win;
        
        LoadSettings();
        
        // Initialize Deep Space Stars with Procedural Clusters
        deep_space_stars.clear();
        cursor_trail.clear(); // Clear trails on init
        std::vector<ImVec2> clusters;
        for(int k=0; k<4; ++k) clusters.push_back(ImVec2((float)(rand()%100)/100.0f, (float)(rand()%100)/100.0f));

        for (int i = 0; i < 200; ++i) {
            Star s;
            if (i < 120) { // 60% in clusters
                ImVec2 center = clusters[rand() % clusters.size()];
                float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
                float dist = (float)(rand() % 100) / 100.0f * 0.15f; // Cluster radius
                s.x = center.x + cosf(angle) * dist;
                s.y = center.y + sinf(angle) * dist;
                // Wrap to keeping 0-1 range
                if(s.x < 0) s.x += 1.0f; if(s.x > 1) s.x -= 1.0f;
                if(s.y < 0) s.y += 1.0f; if(s.y > 1) s.y -= 1.0f;
            } else {
                s.x = (float)(rand() % 1000) / 1000.0f; 
                s.y = (float)(rand() % 1000) / 1000.0f;
            }
            s.speed = 0.05f + ((float)(rand() % 100) / 100.0f) * 0.2f; 
            s.size = 0.5f + ((float)(rand() % 100) / 100.0f) * 1.5f;
            s.brightness = 0.2f + ((float)(rand() % 100) / 100.0f) * 0.8f;
            s.target_brightness = s.brightness;
            deep_space_stars.push_back(s);
        }
        shooting_star.active = false;
        
        // Sync Asset Manager strings from loaded indices
        assets->theme_manager.SyncThemesFromIndices();
        
        ApplyTheme();
        
        if (settings.state.show_system_usage) context->sys_monitor.Init();
        if (settings.state.show_engine_tab && (context->game.sf_active || context->game.lc0_active)) {
             settings.state.show_engine_tab = true;
        }
        
        assets->theme_manager.LoadPieceSet(assets->theme_manager.current_piece_set);
        assets->theme_manager.LoadBoardTheme(assets->theme_manager.current_board_theme);
        context->game.UpdateEngines(true);
    }

    int GetDaysInMonth(int year, int month) {
        if (month == 2) return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) ? 29 : 28;
        if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
        return 31;
    }

    int GetStartWeekday(int year, int month) {
        std::tm time_in = {0};
        time_in.tm_year = year - 1900;
        time_in.tm_mon = month - 1;
        time_in.tm_mday = 1;
        std::mktime(&time_in);
        return time_in.tm_wday; 
    }

    std::string OpenFileDialog(const char* filter, const char* title) {
    #ifdef _WIN32
        char filename[MAX_PATH] = "";
        OPENFILENAMEA ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL; 
        ofn.lpstrFilter = filter;
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = title;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        
        if (GetOpenFileNameA(&ofn)) return std::string(filename);
    #endif
        return "";
    }

    void SaveSettings() {
        settings.Save("Assets/user_data/settings.json", context, assets);
        context->stats.Save("Assets/user_data/stats.json");
        ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
    }

    void LoadSettings() {
        settings.Load("Assets/user_data/settings.json", context, assets);
        context->stats.Load("Assets/user_data/stats.json");
        if (settings.state.show_system_usage) context->sys_monitor.Init();
        if (context->game.sf_active || context->game.lc0_active) settings.state.show_engine_tab = true;
        
        assets->theme_manager.LoadPieceSet(assets->theme_manager.current_piece_set);
        assets->theme_manager.LoadBoardTheme(assets->theme_manager.current_board_theme);
        context->game.UpdateEngines(true);
    }

    void DrawArrow(ImDrawList* draw_list, ImVec2 start, ImVec2 end, ImU32 color) {
        float arrow_width = SQUARE_SIZE * 0.2f; 
        float head_width = SQUARE_SIZE * 0.55f; 
        float head_len = SQUARE_SIZE * 0.45f;   
        
        float dx = end.x - start.x;
        float dy = end.y - start.y;
        float len = sqrtf(dx*dx + dy*dy);
        
        if (len < 1.0f) return;
        float ux = dx / len, uy = dy / len;
        float px = -uy, py = ux;
        
        float neck_x = end.x - ux * head_len;
        float neck_y = end.y - uy * head_len;
        
        if (len > head_len) {
            ImVec2 s1(start.x + px * arrow_width * 0.5f, start.y + py * arrow_width * 0.5f);
            ImVec2 s2(start.x - px * arrow_width * 0.5f, start.y - py * arrow_width * 0.5f);
            ImVec2 s3(neck_x - px * arrow_width * 0.5f, neck_y - py * arrow_width * 0.5f);
            ImVec2 s4(neck_x + px * arrow_width * 0.5f, neck_y + py * arrow_width * 0.5f);
            draw_list->AddQuadFilled(s1, s2, s3, s4, color);
        } else {
            neck_x = start.x; neck_y = start.y;
        }

        ImVec2 h1(neck_x + px * head_width * 0.5f, neck_y + py * head_width * 0.5f);
        ImVec2 h2(neck_x - px * head_width * 0.5f, neck_y - py * head_width * 0.5f);
        draw_list->AddTriangleFilled(h1, h2, end, color);
    }

    void DrawAppBackground() {
        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
        ImVec2 display_size = ImGui::GetIO().DisplaySize;

        // Update starfield origin to board center
        ImVec2 board_center = ImVec2(last_board_pos.x + last_board_size * 0.5f, last_board_pos.y + last_board_size * 0.5f);
        context->game.effects.starfield_origin = board_center;

        if (context->game.effects.enable_starfield && context->game.combo_count >= 20) {
            context->game.effects.DrawStarfield(draw_list, display_size, context->game.combo_count, ImGui::GetIO().DeltaTime, context->GetActivityFactor());
            return;
        }

        if (settings.state.current_app_theme == 0) {
            // Deep Space with Parallax Starfield & Subtle Pulse
            float pulse = (sinf((float)ImGui::GetTime() * 0.5f) * 0.5f + 0.5f);
            ImU32 bg_col = IM_COL32(5 + (int)(3.0f * pulse), 5 + (int)(3.0f * pulse), 8 + (int)(7.0f * pulse), 255);
            draw_list->AddRectFilled(ImVec2(0,0), display_size, bg_col);

            float dt = ImGui::GetIO().DeltaTime;
            for (auto& star : deep_space_stars) {
                // Parallax Movement
                star.x += star.speed * dt * 0.05f; 
                star.y += star.speed * dt * 0.02f;

                // Wrap around
                if (star.x > 1.0f) star.x -= 1.0f;
                if (star.y > 1.0f) star.y -= 1.0f;

                float sx = star.x * display_size.x;
                float sy = star.y * display_size.y;
                
                int alpha = (int)(std::clamp(star.brightness, 0.0f, 1.0f) * 255.0f);
                draw_list->AddCircleFilled(ImVec2(sx, sy), star.size, IM_COL32(255, 255, 255, alpha));
            }

            // Enhanced Aurora / Gradient Horizon
            ImU32 col_trans = IM_COL32(0, 0, 0, 0);
            ImU32 col_horizon = IM_COL32(20, 10, 50, 200); // Brighter Deep Indigo
            
            // Bottom Horizon Gradient
            draw_list->AddRectFilledMultiColor(
                ImVec2(0, display_size.y * 0.5f), 
                display_size, 
                col_trans, col_trans, col_horizon, col_horizon
            );

            // Top Atmosphere Gradient (Subtle)
            ImU32 col_top = IM_COL32(0, 0, 0, 150);
            draw_list->AddRectFilledMultiColor(
                ImVec2(0, 0), 
                ImVec2(display_size.x, display_size.y * 0.3f), 
                col_top, col_top, col_trans, col_trans
            );

            // Interactive Ribbon Cursor Trail (Shooting Star Style)
            ImVec2 mouse_pos = ImGui::GetMousePos();
            
            // Add current point to trail
            CursorTrailParticle p_new;
            p_new.x = mouse_pos.x;
            p_new.y = mouse_pos.y;
            p_new.size = 3.0f; // Base thickness
            p_new.life = 0.4f; // Trail duration in seconds
            cursor_trail.push_back(p_new);

            // Update life and remove dead points
            for (size_t i = 0; i < cursor_trail.size(); ) {
                cursor_trail[i].life -= dt;
                if (cursor_trail[i].life <= 0.0f) {
                    cursor_trail.erase(cursor_trail.begin() + i);
                } else {
                    i++;
                }
            }

            // Draw the Ribbon (Connected Segments)
            if (cursor_trail.size() > 2) {
                for (size_t i = 0; i < cursor_trail.size() - 1; ++i) {
                    const auto& p1 = cursor_trail[i];
                    const auto& p2 = cursor_trail[i+1];
                    
                    float life_factor = p1.life / 0.4f; // 1.0 at head, 0.0 at tail
                    float thickness = p1.size * life_factor;
                    int alpha = (int)(life_factor * 180.0f);
                    
                    ImVec2 pos1(p1.x, p1.y);
                    ImVec2 pos2(p2.x, p2.y);
                    
                    // Outer Glow (Thicker Blue)
                    draw_list->AddLine(pos1, pos2, IM_COL32(100, 180, 255, alpha / 2), thickness * 2.5f);
                    // Inner Core (Thinner White)
                    draw_list->AddLine(pos1, pos2, IM_COL32(255, 255, 255, alpha), thickness);
                }
            }

            // Shooting Stars (Random Events)
            if (shooting_star.active) {
                shooting_star.x += shooting_star.vel_x * dt;
                shooting_star.y += shooting_star.vel_y * dt;
                shooting_star.life -= dt;

                ImVec2 head(shooting_star.x * display_size.x, shooting_star.y * display_size.y);
                ImVec2 tail(head.x - shooting_star.vel_x * display_size.x * 0.05f, head.y - shooting_star.vel_y * display_size.y * 0.05f);
                
                ImU32 col_head = IM_COL32(255, 255, 255, (int)(std::min(1.0f, shooting_star.life) * 255.0f));
                ImU32 col_tail = IM_COL32(255, 255, 255, 0);
                
                draw_list->AddLine(tail, head, col_head, 2.0f);
                
                if (shooting_star.life <= 0.0f || shooting_star.x < -0.1f || shooting_star.x > 1.1f || shooting_star.y < -0.1f || shooting_star.y > 1.1f) {
                    shooting_star.active = false;
                }
            } else {
                if ((rand() % 2000) < 1) { // Rare spawn chance (~once per 10-30s at 60fps)
                    shooting_star.active = true;
                    shooting_star.life = 2.0f;
                    // Spawn from top or left
                    if (rand() % 2 == 0) {
                        shooting_star.x = (float)(rand() % 100) / 100.0f; 
                        shooting_star.y = -0.1f;
                        shooting_star.vel_x = 0.2f + ((float)(rand() % 100) / 100.0f) * 0.3f; // Move right
                        shooting_star.vel_y = 0.2f + ((float)(rand() % 100) / 100.0f) * 0.3f; // Move down
                    } else {
                        shooting_star.x = -0.1f;
                        shooting_star.y = (float)(rand() % 50) / 100.0f; // Top half
                        shooting_star.vel_x = 0.3f + ((float)(rand() % 100) / 100.0f) * 0.4f;
                        shooting_star.vel_y = 0.1f + ((float)(rand() % 100) / 100.0f) * 0.2f;
                    }
                }
            }
        }
        else if (settings.state.current_app_theme == 1) { 
            ImU32 col_top = IM_COL32(20, 20, 40, 255);
            ImU32 col_btm = IM_COL32(40, 20, 60, 255);
            draw_list->AddRectFilledMultiColor(ImVec2(0,0), display_size, col_top, col_top, col_btm, col_btm);
            double t = ImGui::GetTime();
            draw_list->AddCircleFilled(ImVec2(display_size.x*(0.2f + 0.05f * (float)sin(t * 0.5)), display_size.y*(0.3f + 0.05f * (float)cos(t * 0.3))), display_size.y*0.4f, IM_COL32(50, 80, 150, 100));
            draw_list->AddCircleFilled(ImVec2(display_size.x*(0.8f + 0.05f * (float)cos(t * 0.4)), display_size.y*(0.7f + 0.05f * (float)sin(t * 0.6))), display_size.y*0.5f, IM_COL32(100, 50, 120, 100));
        } 
        else if (settings.state.current_app_theme == 4 && assets->theme_manager.bg_enabled) { 
            draw_list->AddRectFilled(ImVec2(0,0), display_size, IM_COL32(0,0,0,255));
            if (assets->theme_manager.bg_texture != 0) {
                float img_w = display_size.x * assets->theme_manager.bg_scale;
                float img_h = display_size.y * assets->theme_manager.bg_scale;
                ImVec2 p_min = ImVec2(display_size.x * 0.5f - img_w * 0.5f, display_size.y * 0.5f - img_h * 0.5f);
                ImVec2 p_max = ImVec2(display_size.x * 0.5f + img_w * 0.5f, display_size.y * 0.5f + img_h * 0.5f);
                draw_list->AddImage((ImTextureID)(intptr_t)assets->theme_manager.bg_texture, p_min, p_max, ImVec2(0,0), ImVec2(1,1), IM_COL32(255,255,255,(int)(assets->theme_manager.bg_brightness * 255.0f)));
            }
        }
        else if (settings.state.current_app_theme == 5) {
            // Enhanced Holographic Nebula (Brighter Version)
            draw_list->AddRectFilled(ImVec2(0,0), display_size, IM_COL32(10, 10, 25, 255)); // Lighter Deep Blue Base
            double t = ImGui::GetTime() * 0.15;
            
            auto DrawNebulaCloud = [&](ImVec2 pos, float radius, ImU32 col) {
                draw_list->AddCircleFilled(pos, radius, col);
            };

            // Layered Nebula Clouds - Increased Alpha for Visibility
            DrawNebulaCloud(ImVec2(display_size.x * 0.5f + (float)sin(t*0.5)*200, display_size.y * 0.5f + (float)cos(t*0.4)*150), 600.0f, IM_COL32(60, 0, 90, 80));   // Deep Purple
            DrawNebulaCloud(ImVec2(display_size.x * 0.2f + (float)cos(t*0.3)*150, display_size.y * 0.2f + (float)sin(t*0.6)*100), 500.0f, IM_COL32(0, 40, 100, 100)); // Dark Blue
            DrawNebulaCloud(ImVec2(display_size.x * 0.8f - (float)sin(t*0.4)*180, display_size.y * 0.8f - (float)cos(t*0.5)*120), 450.0f, IM_COL32(80, 0, 100, 80));  // Magenta
            DrawNebulaCloud(ImVec2(display_size.x * 0.5f + (float)cos(t*0.7)*250, display_size.y * 0.5f - (float)sin(t*0.6)*200), 350.0f, IM_COL32(0, 100, 120, 60)); // Cyan Drift
            DrawNebulaCloud(ImVec2(display_size.x * 0.3f - (float)sin(t*0.2)*100, display_size.y * 0.7f + (float)cos(t*0.3)*100), 400.0f, IM_COL32(30, 30, 80, 80));  // Mid Blue
            
            // Scrolling Grid
            float grid_sz = 80.0f;
            float scroll_x = (float)fmod(t * 20.0, grid_sz);
            float scroll_y = (float)fmod(t * 15.0, grid_sz);
            ImU32 grid_col = IM_COL32(0, 255, 255, 25); // Brighter cyan grid
            
            for (float x = -grid_sz; x < display_size.x + grid_sz; x += grid_sz) 
                draw_list->AddLine(ImVec2(x + scroll_x, 0), ImVec2(x + scroll_x, display_size.y), grid_col, 1.0f);
            for (float y = -grid_sz; y < display_size.y + grid_sz; y += grid_sz) 
                draw_list->AddLine(ImVec2(0, y + scroll_y), ImVec2(display_size.x, y + scroll_y), grid_col, 1.0f);

            // Vignette - Reduced Intensity
            ImU32 vig_col = IM_COL32(0, 0, 0, 180); // Reduced alpha from 255 to 180
            ImU32 vig_trans = IM_COL32(0, 0, 0, 0);
            float border_sz = 150.0f;
            draw_list->AddRectFilledMultiColor(ImVec2(0,0), ImVec2(display_size.x, border_sz), vig_col, vig_col, vig_trans, vig_trans); // Top
            draw_list->AddRectFilledMultiColor(ImVec2(0,display_size.y-border_sz), display_size, vig_trans, vig_trans, vig_col, vig_col); // Bottom
            draw_list->AddRectFilledMultiColor(ImVec2(0,0), ImVec2(border_sz, display_size.y), vig_col, vig_trans, vig_trans, vig_col); // Left
            draw_list->AddRectFilledMultiColor(ImVec2(display_size.x-border_sz,0), display_size, vig_trans, vig_col, vig_col, vig_trans); // Right
        }
    }
    
    void ReloadTheme() {
        static std::string loaded_font = "";
        static int loaded_size = 0;
        static float loaded_scale = 0.0f;
        
        float scale = window ? SDL_GetWindowDisplayScale(window) : 1.0f;
        if (scale <= 0.0f) scale = 1.0f;

        if (loaded_font != assets->theme_manager.current_font_file || loaded_size != assets->theme_manager.current_font_size || loaded_scale != scale) {
            ImGuiIO& io = ImGui::GetIO();
            io.Fonts->Clear();
            
            std::string font_path = "Assets/fonts/" + assets->theme_manager.current_font_file;
            if (!std::filesystem::exists(font_path)) font_path = "../../Assets/fonts/" + assets->theme_manager.current_font_file;
            
            const char* path = std::filesystem::exists(font_path) ? font_path.c_str() : "C:\\Windows\\Fonts\\segoeui.ttf";
            
            font_main = io.Fonts->AddFontFromFileTTF(path, (float)assets->theme_manager.current_font_size * scale);
            font_large = io.Fonts->AddFontFromFileTTF(path, (float)assets->theme_manager.current_font_size * scale * 3.0f); // 3x size for high-res scaling
            
            if (!font_main) font_main = io.Fonts->AddFontDefault();
            if (!font_large) font_large = font_main;
            
            io.FontGlobalScale = 1.0f / scale;
            ImGui_ImplOpenGL3_DestroyFontsTexture();
            ImGui_ImplOpenGL3_CreateFontsTexture();
            
            loaded_font = assets->theme_manager.current_font_file;
            loaded_size = assets->theme_manager.current_font_size;
            loaded_scale = scale;
        }

        ThemeLibrary::ApplyThemeByIndex(settings.state.current_app_theme);
    }

    void ApplyTheme() { theme_dirty = true; }

    void DrawEvalChart() {
        if (!settings.state.show_eval_chart) return;
        ImGui::SetNextWindowSize(ImVec2(600, 200), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Evaluation Graph", &settings.state.show_eval_chart)) {
            auto current_line = context->game.get_current_line_indices();
            if (current_line.empty()) { ImGui::TextDisabled("No evaluation data available."); ImGui::End(); return; }

            std::vector<float> evals;
            std::vector<std::string> eval_strs;
            evals.push_back(0.0f);
            eval_strs.push_back("");
            for (int idx : current_line) {
                evals.push_back(context->game.move_tree[idx].eval);
                eval_strs.push_back(context->game.move_tree[idx].eval_str);
            }

            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImVec2 size = ImGui::GetContentRegionAvail();
            
            float margin_l = 25.0f, margin_b = 15.0f, margin_t = 10.0f, margin_r = 10.0f;
            float graph_x = p.x + margin_l, graph_y = p.y + margin_t;
            float graph_w = size.x - margin_l - margin_r, graph_h = size.y - margin_t - margin_b;

            draw_list->AddRect(ImVec2(graph_x, graph_y), ImVec2(graph_x + graph_w, graph_y + graph_h), IM_COL32(80, 80, 90, 200), 2.0f);
            float zero_y = graph_y + graph_h * 0.5f;
            draw_list->AddLine(ImVec2(graph_x, zero_y), ImVec2(graph_x + graph_w, zero_y), IM_COL32(100, 100, 110, 120));

            auto ValToY = [&](float val) {
                float clamped = std::clamp(val, -8.0f, 8.0f);
                float norm = (clamped + 8.0f) / 16.0f;
                return graph_y + graph_h * (1.0f - norm);
            };

            ImGui::SetWindowFontScale(0.75f);
            auto DrawLabel = [&](float val, const char* txt) {
                ImGui::SetCursorScreenPos(ImVec2(p.x + 2, ValToY(val) - 6));
                ImGui::TextDisabled("%s", txt);
            };
            DrawLabel(8.0f, "+8"); DrawLabel(4.0f, "+4"); DrawLabel(0.0f, " 0"); DrawLabel(-4.0f, "-4"); DrawLabel(-8.0f, "-8");
            ImGui::SetWindowFontScale(1.0f);

            int count = (int)evals.size();
            if (count < 2) { ImGui::End(); return; }
            float x_step = graph_w / (float)(count - 1);
            auto style = context->game.chart_style;

            // --- Precise Fill Pipeline ---
            if (style != GameState::EvalChartStyle::NEON) {
                int subdiv = (style == GameState::EvalChartStyle::HYBRID) ? 4 : 1;
                for (int i = 0; i < count - 1; ++i) {
                    float v1_orig = evals[i];
                    float v2_orig = evals[i+1];
                    for (int s = 0; s < subdiv; ++s) {
                        float t_start = (float)s / (float)subdiv;
                        float t_end = (float)(s + 1) / (float)subdiv;
                        float v1 = v1_orig + (v2_orig - v1_orig) * t_start;
                        float v2 = v1_orig + (v2_orig - v1_orig) * t_end;
                        float x1 = graph_x + (i + t_start) * x_step;
                        float x2 = graph_x + (i + t_end) * x_step;
                        float y1 = ValToY(v1);
                        float y2 = ValToY(v2);

                        auto DrawSegmentFill = [&](float sx1, float sx2, float sy1, float sy2, bool is_pos) {
                            ImU32 col_main, col_fade;
                            if (style == GameState::EvalChartStyle::BASIC) {
                                col_main = col_fade = IM_COL32(139, 233, 253, 80);
                            } else if (style == GameState::EvalChartStyle::DUAL) {
                                col_main = col_fade = is_pos ? IM_COL32(200, 200, 210, 150) : IM_COL32(40, 40, 50, 180);
                            } else {
                                col_main = is_pos ? IM_COL32(50, 220, 120, 180) : IM_COL32(240, 60, 100, 180);
                                col_fade = col_main & 0x00FFFFFF; 
                            }
                            if (style == GameState::EvalChartStyle::HYBRID) {
                                if (is_pos) draw_list->AddRectFilledMultiColor(ImVec2(sx1, std::min(sy1, sy2)), ImVec2(sx2, zero_y), col_main, col_main, col_fade, col_fade);
                                else draw_list->AddRectFilledMultiColor(ImVec2(sx1, zero_y), ImVec2(sx2, std::max(sy1, sy2)), col_fade, col_fade, col_main, col_main);
                            } else {
                                ImVec2 pts[4] = { {sx1, sy1}, {sx2, sy2}, {sx2, zero_y}, {sx1, zero_y} };
                                draw_list->AddConvexPolyFilled(pts, 4, col_main);
                            }
                        };

                        if ((v1 >= 0) == (v2 >= 0)) DrawSegmentFill(x1, x2, y1, y2, v1 >= 0);
                        else {
                            float t_cross = (0.0f - v1) / (v2 - v1);
                            float xc = x1 + t_cross * (x2 - x1);
                            DrawSegmentFill(x1, xc, y1, zero_y, v1 >= 0);
                            DrawSegmentFill(xc, x2, zero_y, y2, v2 >= 0);
                        }
                    }
                }
            }

            // --- Trend Line ---
            for (int i = 0; i < count - 1; ++i) {
                ImVec2 p1(graph_x + i * x_step, ValToY(evals[i]));
                ImVec2 p2(graph_x + (i + 1) * x_step, ValToY(evals[i+1]));
                if (style == GameState::EvalChartStyle::NEON) {
                    draw_list->AddLine(p1, p2, IM_COL32(139, 233, 253, 40), 7.0f);
                    draw_list->AddLine(p1, p2, IM_COL32(139, 233, 253, 100), 3.0f);
                    draw_list->AddLine(p1, p2, IM_COL32(255, 255, 255, 255), 1.5f);
                } else {
                    draw_list->AddLine(p1, p2, (style == GameState::EvalChartStyle::HYBRID) ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 255, 255, 180), 1.5f);
                }
            }

            int current_history_idx = count - 1;
            if (current_history_idx < count) {
                ImVec2 cur_p(graph_x + current_history_idx * x_step, ValToY(evals[current_history_idx]));
                draw_list->AddCircleFilled(cur_p, 4.5f, IM_COL32(255, 184, 108, 255));
                draw_list->AddCircle(cur_p, 6.5f, IM_COL32(255, 255, 255, 150), 0, 1.5f);
            }

            ImGui::SetCursorScreenPos(ImVec2(graph_x, graph_y));
            ImGui::InvisibleButton("GraphHitbox", ImVec2(graph_w, graph_h));
            if (ImGui::IsItemHovered()) {
                int idx = std::clamp((int)round((ImGui::GetMousePos().x - graph_x) / x_step), 0, count - 1);
                draw_list->AddLine(ImVec2(graph_x + idx * x_step, graph_y), ImVec2(graph_x + idx * x_step, graph_y + graph_h), IM_COL32(255, 255, 255, 60));
                std::string eval_txt = (idx < (int)eval_strs.size()) ? eval_strs[idx] : "...";
                ImGui::SetTooltip("Move %d (%s)\nEval: %s", (idx + 1) / 2, (idx % 2 == 0 ? "B" : "W"), eval_txt.c_str());
                if (ImGui::IsMouseDown(0) && idx > 0) {
                    context->game.jump_to_node(current_line[idx - 1]);
                } else if (ImGui::IsMouseDown(0) && idx == 0) {
                    context->game.jump_to_node(-1);
                }
            }
        }
        ImGui::End();
    }

    void DrawBatchAnalysisDashboard() {
        if (!settings.state.show_batch_dashboard) return;
        ImGui::SetNextWindowSize(ImVec2(800, 550), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Adaptive Batch Analysis", &settings.state.show_batch_dashboard)) {
            
            if (context->batch.active) {
                if (ImGui::Button("Stop Process", ImVec2(120, 0))) context->StopBatchAnalysis();
                ImGui::SameLine();
                float progress = (float)context->batch.report.games_scouted / (float)context->batch.report.total_games;
                char buf[64]; snprintf(buf, sizeof(buf), "Pass %d: Analyzing Game %d / %d", context->batch.pass, context->batch.current_game_idx + 1, context->batch.report.total_games);
                ImGui::ProgressBar(progress, ImVec2(-1, 0), buf);
            } else {
                static int depth = 12; static bool deep_dive = true;
                ImGui::Text("Batch Config:"); ImGui::SameLine();
                ImGui::PushItemWidth(100); ImGui::SliderInt("##depth", &depth, 8, 16, "Depth: %d"); ImGui::PopItemWidth(); ImGui::SameLine();
                ImGui::Checkbox("Deep Dive", &deep_dive); ImGui::SameLine();
                if (ImGui::Button("Scan", ImVec2(80, 0))) context->StartBatchAnalysis(depth, deep_dive);
            }

            ImGui::Separator();

            if (ImGui::BeginTabBar("BatchTabs")) {
                if (ImGui::BeginTabItem("Move Heatmap")) {
                    ImGui::BeginChild("HeatmapScroll");
                    if (context->batch.report.summaries.empty()) ImGui::TextDisabled("Run analysis to generate intelligence heatmap.");
                    
                    for (const auto& gs : context->batch.report.summaries) {
                        ImGui::PushID(gs.game_idx);
                        float sz = 14.0f, spacing = 2.0f, row_h = sz + 1.0f, name_w = 40.0f;
                        ImVec2 p = ImGui::GetCursorScreenPos();
                        ImDrawList* dl = ImGui::GetWindowDrawList();

                        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y - 3));
                        ImGui::Text("%s", gs.white_name_cached.c_str());
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s (White)", gs.white.c_str());
                        
                        for (int m = 0; m < (int)gs.move_qualities.size(); m += 2) {
                            int q = gs.move_qualities[m];
                            ImU32 col = IM_COL32(50, 200, 100, 200); 
                            if (q == 1) col = IM_COL32(255, 255, 0, 200); 
                            if (q == 2) col = IM_COL32(255, 150, 0, 200); 
                            if (q == 3) col = IM_COL32(255, 50, 50, 255); 
                            if (q == 4) col = IM_COL32(255, 215, 0, 255); 
                            if (q == 5) col = IM_COL32(0, 255, 255, 255); 

                            float x_pos = p.x + name_w + (m/2) * (sz + spacing);
                            ImVec2 p_min = ImVec2(x_pos, p.y);
                            ImVec2 p_max = ImVec2(p_min.x + sz, p_min.y + sz);
                            dl->AddRectFilled(p_min, p_max, col, 2.0f);
                            
                            ImGui::SetCursorScreenPos(p_min);
                            ImGui::PushID(m);
                            if (ImGui::InvisibleButton("##w_btn", ImVec2(sz, sz))) {
                                context->LoadImportedGame(gs.game_idx); context->game.jump_to_move(m + 1); assets->PlaySound(assets->snd_confirm);
                            }
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("White Move %d\nEval: %.2f", (m/2)+1, gs.move_evals[m]);
                            ImGui::PopID();
                        }

                        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + row_h - 3));
                        ImGui::Text("%s", gs.black_name_cached.c_str());
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s (Black)", gs.black.c_str());

                        for (int m = 1; m < (int)gs.move_qualities.size(); m += 2) {
                            int q = gs.move_qualities[m];
                            ImU32 col = IM_COL32(50, 200, 100, 200); 
                            if (q == 1) col = IM_COL32(255, 255, 0, 200); 
                            if (q == 2) col = IM_COL32(255, 150, 0, 200); 
                            if (q == 3) col = IM_COL32(255, 50, 50, 255); 
                            if (q == 4) col = IM_COL32(255, 215, 0, 255); 
                            if (q == 5) col = IM_COL32(0, 255, 255, 255); 

                            float x_pos = p.x + name_w + (m/2) * (sz + spacing);
                            ImVec2 p_min = ImVec2(x_pos, p.y + row_h);
                            ImVec2 p_max = ImVec2(p_min.x + sz, p_min.y + sz);
                            dl->AddRectFilled(p_min, p_max, col, 2.0f);
                            
                            ImGui::SetCursorScreenPos(p_min);
                            ImGui::PushID(m);
                            if (ImGui::InvisibleButton("##b_btn", ImVec2(sz, sz))) {
                                context->LoadImportedGame(gs.game_idx); context->game.jump_to_move(m + 1); assets->PlaySound(assets->snd_confirm);
                            }
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Black Move %d\nEval: %.2f", (m/2)+1, gs.move_evals[m]);
                            ImGui::PopID();
                        }
                        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetWindowPos().x + 8, p.y + row_h * 2 + 6.0f));
                        ImGui::Separator();
                        ImGui::PopID();
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Tactical Report")) {
                    ImGui::Columns(2, "ReportCols", true);
                    ImGui::SetColumnWidth(0, 300);
                    static int selected_report_idx = 0;
                    ImGui::TextColored(ImVec4(0,1,1,1), "GLOBAL INTELLIGENCE");
                    ImGui::Separator();
                    ImGui::BulletText("Total Games Scouted: %d", context->batch.report.games_scouted);
                    ImGui::BulletText("Total Blunders: %d", context->batch.report.total_blunders);
                    ImGui::BulletText("Total Brilliants: %d", context->batch.report.total_brilliants);
                    
                    if (!context->batch.report.summaries.empty()) {
                        ImGui::Dummy(ImVec2(0, 10));
                        ImGui::TextColored(ImVec4(1,1,0,1), "DETAILED PHASE BREAKDOWN");
                        if (selected_report_idx >= (int)context->batch.report.summaries.size()) selected_report_idx = 0;
                        const auto& gs = context->batch.report.summaries[selected_report_idx];
                        
                        auto PhaseRow = [&](const char* label, float acc) {
                            ImGui::Text("%s:", label); ImGui::SameLine(120);
                            ImVec4 c = (acc >= 90) ? ImVec4(0.4f,1,0.4f,1) : (acc >= 75 ? ImVec4(1,1,0.4f,1) : ImVec4(1,0.4f,0.4f,1));
                            ImGui::TextColored(c, "%.1f%%", acc);
                        };
                        PhaseRow("Opening", gs.opAcc); PhaseRow("Middlegame", gs.midAcc); PhaseRow("Endgame", gs.endAcc);
                        
                        ImGui::Separator();
                        ImGui::TextColored(ImVec4(1,0.5f,1,1), "ADVANCED METRICS");
                        ImGui::BulletText("Fighting Spirit: %.1f%%", gs.fighting_spirit);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Accuracy maintained while in losing positions (-2.0 to -10.0)");
                        ImGui::BulletText("Punishment: %.1f%%", gs.punishment);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Efficiency in capitalizing on opponent's blunders");
                    }
                    
                    ImGui::NextColumn();
                    ImGui::TextColored(ImVec4(0,1,1,1), "PER-GAME PERFORMANCE");
                    ImGui::BeginChild("SummariesList");
                    for (int i=0; i < (int)context->batch.report.summaries.size(); ++i) {
                        const auto& gs = context->batch.report.summaries[i];
                        ImGui::PushID(i);
                        float acc = gs.accuracy; const char* tier = "POOR"; ImVec4 acc_col = ImVec4(1,0.4f,0.4f,1);
                        if (acc >= 95) { tier = "ELITE"; acc_col = ImVec4(0.4f,1,1,1); }
                        else if (acc >= 90) { tier = "HIGH"; acc_col = ImVec4(0.4f,1,0.4f,1); }
                        else if (acc >= 80) { tier = "GOOD"; acc_col = ImVec4(0.8f,1,0.4f,1); }
                        else if (acc >= 70) { tier = "OKAY"; acc_col = ImVec4(1,1,0.4f,1); }

                        bool is_selected = (selected_report_idx == i);
                        if (is_selected) ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 215, 0, 255));

                        ImGui::Text("Game %d:", gs.game_idx + 1); ImGui::SameLine(); ImGui::TextColored(acc_col, "%.1f%% [%s]", acc, tier);
                        ImGui::TextDisabled(" %s vs %s", gs.white.c_str(), gs.black.c_str());
                        
                        auto Badge = [&](const char* lbl, int count, ImVec4 col) { if (count <= 0) return; ImGui::SameLine(); ImGui::TextColored(col, "%s:%d", lbl, count); };
                        Badge("!!", gs.brilliants, ImVec4(0,1,1,1)); Badge("!", gs.greats, ImVec4(1,0.8f,0,1));
                        Badge("??", gs.blunders, ImVec4(1,0,0,1)); Badge("?", gs.mistakes, ImVec4(1,0.5f,0,1));
                        
                        ImGui::TextDisabled(" ACPL: %.0f", gs.acpl); ImGui::SameLine();
                        if (ImGui::Button("Review")) { context->LoadImportedGame(gs.game_idx); assets->PlaySound(assets->snd_confirm); }
                        ImGui::SameLine();
                        if (ImGui::Button("Details")) { selected_report_idx = i; assets->PlaySound(assets->snd_select); }
                        
                        if (is_selected) ImGui::PopStyleColor();
                        ImGui::Separator();
                        ImGui::PopID();
                    }
                    ImGui::EndChild(); ImGui::Columns(1); ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Discovery Feed")) {
                    ImGui::BeginChild("FeedLog");
                    for (const auto& log : context->batch.report.feed_log) ImGui::TextWrapped("> %s", log.c_str());
                    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
                    ImGui::EndChild(); ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    void DrawMoveList() {
        if (!settings.state.show_move_list) return;
        ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Move List", &settings.state.show_move_list)) {
            if (context->imported_games.size() > 1) {
                ImGui::SeparatorText("SELECT GAME");
                std::string current_label = "Game " + std::to_string(context->current_game_idx + 1);
                if (context->current_game_idx >= 0) current_label += ": " + context->imported_games[context->current_game_idx].white_player + " vs " + context->imported_games[context->current_game_idx].black_player;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::BeginCombo("##game_selector", current_label.c_str())) {
                    for (int i = 0; i < (int)context->imported_games.size(); ++i) {
                        const auto& g = context->imported_games[i];
                        std::string label = std::to_string(i + 1) + ". " + g.white_player + " vs " + g.black_player;
                        bool is_selected = (context->current_game_idx == i);
                        if (ImGui::Selectable(label.c_str(), is_selected)) { context->LoadImportedGame(i); assets->PlaySound(assets->snd_confirm); }
                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::Spacing();
            }

            if (context->analysis_active) {
                if (ImGui::Button("Stop Analysis", ImVec2(-1, 0))) context->StopFullGameAnalysis();
                float progress = 0.0f;
                auto main_branch = context->game.get_main_branch_indices();
                if (!main_branch.empty()) progress = (float)context->analysis_idx / (float)main_branch.size();
                char overlay[64];
                if (context->analysis_pass == 1) snprintf(overlay, sizeof(overlay), "Pass 1: %d / %d (%.0f%%)", context->analysis_idx, (int)main_branch.size(), progress * 100.0f);
                else snprintf(overlay, sizeof(overlay), "Pass 2: %d Critical Moments Left", (int)context->deep_indices.size());
                ImGui::ProgressBar(progress, ImVec2(-1, 0), overlay);
            } else {
                static int selected_mode = 2; const char* modes[] = { "Quick (D14)", "Deep (D24)", "Smart Hybrid" };
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
                ImGui::Combo("##analysis_mode", &selected_mode, modes, IM_ARRAYSIZE(modes)); ImGui::SameLine();
                if (ImGui::Button("Analyze", ImVec2(-1, 0))) context->StartFullGameAnalysis((GameContext::AnalysisMode)selected_mode);
            }
            ImGui::Separator();

            if (ImGui::BeginChild("MovesTree", ImVec2(0, 0), true)) {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                static int last_scrolled_node = -1;
                
                std::vector<int> roots;
                for (int i = 0; i < (int)context->game.move_tree.size(); ++i) {
                    if (context->game.move_tree[i].parent_idx == -1) roots.push_back(i);
                }

                if (!roots.empty()) {
                    chess::Board start_b(context->game.start_fen);
                    int base_move_num = start_b.fullMoveNumber();
                    int start_ply = (start_b.sideToMove() == chess::Color::WHITE) ? 0 : 1;
                    
                    auto GetMoveNum = [&](int p) { return base_move_num + ((p - start_ply) / 2); };
                    
                    auto render_variation_inline = [&](auto& self, int start_node, int s_ply, float indent_x, bool& first_on_line) -> void {
                        auto HandleWrap = [&](float width) {
                            if (first_on_line) {
                                ImGui::SetCursorPosX(indent_x);
                                first_on_line = false;
                            } else {
                                float max_x = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
                                if (ImGui::GetCursorScreenPos().x + width > max_x - 5.0f) {
                                    ImGui::Dummy(ImVec2(0, 0)); // Flush pending SameLine cleanly
                                    indent_x = ImGui::GetCursorStartPos().x + 20.0f; // Reset indentation to the far left to maximize space!
                                    ImGui::SetCursorPosX(indent_x);
                                }
                            }
                        };

                        auto DrawInlineText = [&](const std::string& text) {
                            ImVec2 text_sz = ImGui::CalcTextSize(text.c_str());
                            HandleWrap(text_sz.x);
                            ImGui::TextDisabled("%s", text.c_str());
                            ImGui::SameLine(0, 4.0f);
                        };

                        auto DrawInlineNode = [&](const std::string& move_san, int node_idx) {
                            auto& node = context->game.move_tree[node_idx];
                            std::string text = move_san;
                            if (!node.eval_str.empty()) text += "  " + node.eval_str;
                            else {
                                if (!node.live_sf_eval.empty()) text += "  " + node.live_sf_eval;
                                if (!node.live_lc0_eval.empty()) text += "  " + node.live_lc0_eval;
                            }
                            
                            ImVec2 text_sz = ImGui::CalcTextSize(text.c_str());
                            HandleWrap(text_sz.x);
                            
                            bool is_active = (context->game.current_node_idx == node_idx);
                            if (is_active) {
                                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
                                ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 100, 0, 100));
                            } else {
                                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(170, 170, 170, 255));
                            }
                            
                            ImGui::PushID(node_idx);
                            if (ImGui::Selectable(text.c_str(), is_active, 0, text_sz)) context->game.jump_to_node(node_idx);
                            ImGui::PopID();
                            
                            if (is_active) { 
                                ImGui::PopStyleColor(2); 
                                if (last_scrolled_node != node_idx) {
                                    ImGui::SetScrollHereY(0.5f);
                                    last_scrolled_node = node_idx;
                                }
                            } else {
                                ImGui::PopStyleColor();
                            }
                            
                            ImGui::SameLine(0, 4.0f);
                        };

                        DrawInlineText("(");
                        int curr = start_node;
                        int ply = s_ply;
                        
                        while (curr != -1) {
                            auto& node = context->game.move_tree[curr];
                            std::string text;
                            if (ply % 2 == 0) text = std::to_string(GetMoveNum(ply)) + ". " + node.san;
                            else if (curr == start_node) text = std::to_string(GetMoveNum(ply)) + "... " + node.san;
                            else text = node.san;
                            
                            DrawInlineNode(text, curr);
                            
                            if (node.children.size() > 1) {
                                for (size_t i = 1; i < node.children.size(); ++i) {
                                    self(self, node.children[i], ply + 1, indent_x + 10.0f, first_on_line);
                                }
                            }
                            
                            if (!node.children.empty()) { curr = node.children[0]; ply++; }
                            else curr = -1;
                        }
                        DrawInlineText(")");
                    };

                    auto DrawMainNode = [&](const std::string& move_san, int node_idx) {
                        auto& node = context->game.move_tree[node_idx];
                        std::string text = move_san;
                        
                        if (!node.eval_str.empty()) {
                            text += "  " + node.eval_str;
                        } else {
                            if (!node.live_sf_eval.empty()) text += "  " + node.live_sf_eval;
                            if (!node.live_lc0_eval.empty()) text += "  " + node.live_lc0_eval;
                        }
                        
                        bool is_active = (context->game.current_node_idx == node_idx);
                        if (is_active) {
                            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
                            ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 100, 0, 100));
                        }
                        
                        ImVec2 text_sz = ImGui::CalcTextSize(text.c_str());
                        ImGui::PushID(node_idx);
                        if (ImGui::Selectable(text.c_str(), is_active, 0, text_sz)) context->game.jump_to_node(node_idx);
                        ImGui::PopID();
                        
                        if (is_active) { 
                            ImGui::PopStyleColor(2); 
                            if (last_scrolled_node != node_idx) {
                                ImGui::SetScrollHereY(0.5f);
                                last_scrolled_node = node_idx;
                            }
                        }
                    };

                    int curr = roots[0];
                    int ply = start_ply;
                    float start_x = ImGui::GetCursorStartPos().x;
                    float col1_x = 40.0f;
                    float col2_x = std::max(120.0f, ImGui::GetContentRegionAvail().x / 2.0f + 10.0f);

                    for (size_t i = 1; i < roots.size(); ++i) {
                        bool first_on_line = true;
                        render_variation_inline(render_variation_inline, roots[i], start_ply, start_x + 10.0f, first_on_line);
                        ImGui::NewLine();
                    }

                    while (curr != -1) {
                        auto& node = context->game.move_tree[curr];
                        bool is_white = (ply % 2 == 0);
                        
                        if (is_white) {
                            ImGui::TextDisabled("%d.", GetMoveNum(ply));
                            ImGui::SameLine(); ImGui::SetCursorPosX(start_x + col1_x);
                            
                            DrawMainNode(node.san, curr);
                            
                            std::vector<int> white_vars;
                            if (node.parent_idx == -1) {
                                for (size_t i = 1; i < roots.size(); ++i) white_vars.push_back(roots[i]);
                            } else {
                                auto& pnode = context->game.move_tree[node.parent_idx];
                                for (size_t i = 1; i < pnode.children.size(); ++i) white_vars.push_back(pnode.children[i]);
                            }
                            
                            if (!white_vars.empty()) {
                                ImGui::NewLine();
                                for (int v : white_vars) {
                                    bool first_on_line = true;
                                    render_variation_inline(render_variation_inline, v, ply, start_x + 15.0f, first_on_line);
                                    ImGui::NewLine();
                                }
                            } else {
                                ImGui::SameLine(); 
                            }
                            
                            if (node.children.empty()) {
                                if (white_vars.empty()) ImGui::NewLine();
                                break;
                            }
                            
                            int black_curr = node.children[0];
                            auto& bnode = context->game.move_tree[black_curr];
                            
                            if (!white_vars.empty()) {
                                ImGui::TextDisabled("%d...", GetMoveNum(ply));
                                ImGui::SameLine(); 
                            }
                            
                            ImGui::SetCursorPosX(start_x + col2_x);
                            DrawMainNode(bnode.san, black_curr);
                            
                            std::vector<int> black_vars;
                            for (size_t i = 1; i < node.children.size(); ++i) black_vars.push_back(node.children[i]);
                            
                            if (!black_vars.empty()) {
                                ImGui::NewLine();
                                for (int v : black_vars) {
                                    bool first_on_line = true;
                                    render_variation_inline(render_variation_inline, v, ply + 1, start_x + 15.0f, first_on_line);
                                    ImGui::NewLine();
                                }
                            } else {
                                ImGui::NewLine();
                            }
                            
                            curr = bnode.children.empty() ? -1 : bnode.children[0];
                            ply += 2;
                        } else {
                            ImGui::TextDisabled("%d...", GetMoveNum(ply));
                            ImGui::SameLine(); ImGui::SetCursorPosX(start_x + col2_x);
                            DrawMainNode(node.san, curr);
                            
                            std::vector<int> black_vars;
                            if (node.parent_idx == -1) {
                                for (size_t i = 1; i < roots.size(); ++i) black_vars.push_back(roots[i]);
                            } else {
                                auto& pnode = context->game.move_tree[node.parent_idx];
                                for (size_t i = 1; i < pnode.children.size(); ++i) black_vars.push_back(pnode.children[i]);
                            }
                            
                            if (!black_vars.empty()) {
                                ImGui::NewLine();
                                for (int v : black_vars) {
                                    bool first_on_line = true;
                                    render_variation_inline(render_variation_inline, v, ply, start_x + 15.0f, first_on_line);
                                    ImGui::NewLine();
                                }
                            } else {
                                ImGui::NewLine();
                            }
                            
                            curr = node.children.empty() ? -1 : node.children[0];
                            ply++;
                        }
                    }
                }
                ImGui::PopStyleVar();
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    void DrawChessBoard() {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 board_top_left = ImGui::GetCursorScreenPos();
        ImVec2 mouse_pos = ImGui::GetMousePos();

        context->game.effects.Precalculate(!context->game.is_flipped);

        for (const auto& pe : context->game.effects.pending_effects) {
            if (pe.type == EffectsManager::EffectType::FIREWORK) context->game.effects.SpawnFirework(ImVec2(board_top_left.x + BOARD_SIZE/2, board_top_left.y + BOARD_SIZE/2), pe.color);
            else if (pe.type == EffectsManager::EffectType::SPARK && pe.sq != chess::Square::underlying::NO_SQ) {
                int c, r; BoardGeometry::GetVisualFromSquare(pe.sq, cached_cos, cached_sin, c, r);
                ImVec2 sp = BoardGeometry::GetSquareScreenPos(board_top_left, c, r, SQUARE_SIZE, context->game.effects.GetShakeOffset());
                context->game.effects.SpawnSparks(ImVec2(sp.x + SQUARE_SIZE/2, sp.y + SQUARE_SIZE/2), pe.color);
            }
        }
        context->game.effects.pending_effects.clear();

        if (context->game.current_node_idx != last_cached_history_idx || context->game.start_fen != last_cached_start_fen) {
            cached_display_board = chess::Board(context->game.start_fen);
            auto line = context->game.get_current_line_indices();
            for (int idx : line) cached_display_board.makeMove(context->game.move_tree[idx].move);
            last_cached_history_idx = context->game.current_node_idx; last_cached_start_fen = context->game.start_fen;
        }

        chess::Board display_board = cached_display_board;
        if (context->game.preview_mode) {
            for (int i = 0; i < (context->game.preview_animating ? context->game.preview_anim_idx : (int)context->game.preview_moves.size()); ++i) display_board.makeMove(context->game.preview_moves[i]);
        }

        ImU32 col_light, col_dark;
        if (settings.state.current_app_theme == 1) { 
            col_light = IM_COL32(255, 255, 255, 180); 
            col_dark  = IM_COL32(70, 90, 120, 160);   
            draw_list->AddRectFilled(board_top_left, ImVec2(board_top_left.x + BOARD_SIZE, board_top_left.y + BOARD_SIZE), IM_COL32(100, 150, 255, 40), 18.0f);
        } else if (settings.state.current_app_theme == 2) { 
            col_light = IM_COL32(13, 13, 20, 255); 
            col_dark  = IM_COL32(26, 26, 36, 255); 
            draw_list->AddRect(board_top_left, ImVec2(board_top_left.x + BOARD_SIZE, board_top_left.y + BOARD_SIZE), IM_COL32(255, 0, 200, 100), 0.0f, 0, 2.0f);
        } else { 
            col_light = IM_COL32(235, 236, 239, 255); 
            col_dark  = IM_COL32(112, 135, 163, 255); 
            draw_list->AddRect(board_top_left, ImVec2(board_top_left.x + BOARD_SIZE, board_top_left.y + BOARD_SIZE), IM_COL32(255, 255, 255, 30), 12.0f, 0, 4.0f);
        }

        ImGui::InvisibleButton("BoardArea", ImVec2(BOARD_SIZE, BOARD_SIZE));
        
        if (context->game.preview_mode) {
            draw_list->AddText(ImGui::GetFont(), 24.0f, ImVec2(board_top_left.x + 10, board_top_left.y + 10), IM_COL32(255, 0, 0, 200), "PREVIEW MODE");
        }
        
        bool board_hovered = ImGui::IsItemHovered();
        bool mouse_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        bool mouse_released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
        bool right_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        bool right_released = ImGui::IsMouseReleased(ImGuiMouseButton_Right);
        bool right_dragging = ImGui::IsMouseDragging(ImGuiMouseButton_Right);

        last_board_pos = board_top_left; last_board_size = BOARD_SIZE;

        chess::Square hovered_sq = board_hovered ? BoardGeometry::GetSquareAtScreenPos(board_top_left, mouse_pos, SQUARE_SIZE, cached_cos, cached_sin, context->game.effects.GetShakeOffset()) : chess::Square::underlying::NO_SQ;
        context->game.effects.UpdateHover(hovered_sq);

        // Single pass top-to-bottom for correct 2.5D depth sorting
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                chess::Square sq = BoardGeometry::GetSquareFromVisual(col, row, cached_cos, cached_sin);
                float dep = context->game.effects.GetSquareDepression(sq);
                
                ImVec2 orig_pos = BoardGeometry::GetSquareScreenPos(board_top_left, col, row, SQUARE_SIZE, context->game.effects.GetShakeOffset());
                ImVec2 pos = orig_pos;
                pos.y += dep * 0.3f;
                
                ImU32 base_col = (row + col) % 2 == 0 ? col_light : col_dark;
                ImU32 tint_col = IM_COL32_WHITE;

                if (context->game.effects.enable_square_colors) {
                    float r = context->game.effects.board_tint_r;
                    float g = context->game.effects.board_tint_g;
                    float b = context->game.effects.board_tint_b;

                    if (context->game.effects.board_hue_shift > 0.0f || context->game.effects.board_brightness != 1.0f) {
                        float h, s, v;
                        ImGui::ColorConvertRGBtoHSV(r, g, b, h, s, v);
                        h = fmodf(h + context->game.effects.board_hue_shift, 1.0f);
                        if (h < 0.0f) h += 1.0f;
                        v *= context->game.effects.board_brightness;
                        v = std::clamp(v, 0.0f, 1.0f);
                        ImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b);
                    }

                    if (assets->theme_manager.board_texture != 0) {
                        tint_col = IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), 255);
                    } else {
                        ImVec4 bc = ImGui::ColorConvertU32ToFloat4(base_col);
                        base_col = IM_COL32((int)(bc.x * r * 255), (int)(bc.y * g * 255), (int)(bc.z * b * 255), 255);
                    }
                }
                
                // If depressed, draw a dark "hole" base first so the gap is filled
                if (dep > 0.1f) {
                    draw_list->AddRectFilled(orig_pos, ImVec2(orig_pos.x + SQUARE_SIZE, orig_pos.y + SQUARE_SIZE), IM_COL32(0, 0, 0, 150));
                }

                ImU32 edge_col;
                if (context->game.effects.enable_square_colors) {
                    edge_col = IM_COL32(
                        (int)(context->game.effects.edge_color_r * 255),
                        (int)(context->game.effects.edge_color_g * 255),
                        (int)(context->game.effects.edge_color_b * 255),
                        255
                    );
                } else {
                    // Unconditionally calculate the pure mean of the light and dark colors
                    ImVec4 cl = ImGui::ColorConvertU32ToFloat4(col_light);
                    ImVec4 cd = ImGui::ColorConvertU32ToFloat4(col_dark);
                    edge_col = ImGui::ColorConvertFloat4ToU32(ImVec4((cl.x + cd.x) * 0.5f, (cl.y + cd.y) * 0.5f, (cl.z + cd.z) * 0.5f, 1.0f));
                }
                
                // Draw 3D sides (extrusion) if lifted
                if (dep < -0.1f) {
                    draw_list->AddRectFilled(ImVec2(pos.x, pos.y + SQUARE_SIZE - 1.0f), ImVec2(pos.x + SQUARE_SIZE, orig_pos.y + SQUARE_SIZE), edge_col);
                    // Draw a subtle drop shadow on the ground
                    draw_list->AddRectFilled(ImVec2(orig_pos.x + 2.0f, orig_pos.y + 2.0f), ImVec2(orig_pos.x + SQUARE_SIZE + 2.0f, orig_pos.y + SQUARE_SIZE + 2.0f), IM_COL32(0, 0, 0, 50));
                }

                // Draw the top face (the actual tile)
                if (assets->theme_manager.board_texture != 0) {
                    ImVec2 uv_min = ImVec2((float)col / 8.0f, (float)row / 8.0f);
                    ImVec2 uv_max = ImVec2((float)(col + 1) / 8.0f, (float)(row + 1) / 8.0f);
                    draw_list->AddImage((ImTextureID)(intptr_t)assets->theme_manager.board_texture, pos, ImVec2(pos.x + SQUARE_SIZE, pos.y + SQUARE_SIZE), uv_min, uv_max, tint_col);
                } else {
                    draw_list->AddRectFilled(pos, ImVec2(pos.x + SQUARE_SIZE, pos.y + SQUARE_SIZE), base_col);
                }

                // Wave edge illumination
                if (dep < -0.1f && context->game.effects.enable_domino_wave && context->game.effects.domino_wave_glow > 0.01f) {
                    float glow_intensity = std::min(1.0f, (-dep) / 5.0f) * context->game.effects.domino_wave_glow;
                    int alpha = std::clamp((int)(glow_intensity * 150.0f), 0, 255);
                    draw_list->AddRectFilled(pos, ImVec2(pos.x + SQUARE_SIZE, pos.y + SQUARE_SIZE), IM_COL32(255, 255, 255, alpha));
                }

                if (context->drag_state.is_dragging) {
                    int src_col, src_row;
                    BoardGeometry::GetVisualFromSquare(context->drag_state.source_sq, cached_cos, cached_sin, src_col, src_row);
                    if (src_col == col && src_row == row) {
                        draw_list->AddRectFilled(pos, ImVec2(pos.x + SQUARE_SIZE, pos.y + SQUARE_SIZE), IM_COL32(255, 255, 0, 100));
                    }
                }
            }
        }

        // --- Living Check Effect Rendering ---
        if (context->game.board.inCheck()) {
            float pulse_alpha = (sinf((float)ImGui::GetTime() * 5.0f) * 0.5f + 0.5f) * 100.0f + 30.0f; // Varies between 30 and 130
            
            // Draw king's square glow
            chess::Square ksq = context->game.board.kingSq(context->game.board.sideToMove());
            if (ksq != chess::Square::underlying::NO_SQ) {
                int col, row;
                BoardGeometry::GetVisualFromSquare(ksq, cached_cos, cached_sin, col, row);
                ImVec2 pos = BoardGeometry::GetSquareScreenPos(board_top_left, col, row, SQUARE_SIZE, context->game.effects.GetShakeOffset());
                draw_list->AddRectFilled(pos, ImVec2(pos.x + SQUARE_SIZE, pos.y + SQUARE_SIZE), IM_COL32(255, 0, 0, (int)pulse_alpha));
            }
        }

        if (settings.state.show_coordinates) {
             for (int i = 0; i < 8; ++i) {
                // Ranks (Vertical along left edge)
                int rank = context->game.is_flipped ? (i + 1) : (8 - i);
                char rank_str[2] = { (char)('0' + rank), '\0' };
                
                ImVec2 rank_pos = BoardGeometry::GetSquareScreenPos(board_top_left, 0, i, SQUARE_SIZE, context->game.effects.GetShakeOffset());
                ImU32 rank_col = (i % 2 == 0) ? col_dark : col_light; 
                draw_list->AddText(ImGui::GetFont(), SQUARE_SIZE * 0.25f, ImVec2(rank_pos.x + 2, rank_pos.y + 2), rank_col, rank_str);

                // Files (Horizontal along bottom edge)
                char file = context->game.is_flipped ? ('h' - i) : ('a' + i);
                char file_str[2] = { file, '\0' };
                
                ImVec2 file_pos = BoardGeometry::GetSquareScreenPos(board_top_left, i, 7, SQUARE_SIZE, context->game.effects.GetShakeOffset());
                ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(SQUARE_SIZE * 0.25f, FLT_MAX, 0.0f, file_str);
                ImU32 file_col = ((7 + i) % 2 == 0) ? col_dark : col_light;
                draw_list->AddText(ImGui::GetFont(), SQUARE_SIZE * 0.25f, ImVec2(file_pos.x + SQUARE_SIZE - textSize.x - 2, file_pos.y + SQUARE_SIZE - textSize.y - 2), file_col, file_str);
             }
        }
        
        context->game.effects.DrawFlashes(draw_list, SQUARE_SIZE);
        context->game.effects.DrawRipples(draw_list);
        context->game.effects.DrawShockwaves(draw_list);
        context->game.effects.DrawGhosts(draw_list, assets->theme_manager.piece_textures, SQUARE_SIZE);
        context->game.effects.Draw(draw_list);

        if (context->game.hint_level > 0 && context->puzzle_manager.GetCurrentPuzzle() && context->puzzle_manager.current_move_idx < context->puzzle_manager.GetCurrentPuzzle()->san_moves.size()) {
            std::string expected_san = context->puzzle_manager.GetCurrentPuzzle()->san_moves[context->puzzle_manager.current_move_idx];
            chess::Move expected_move = chess::uci::parseSan(display_board, expected_san, cached_legal_moves);
            
            if (expected_move == chess::Move::NO_MOVE && (expected_san.back() == '+' || expected_san.back() == '#')) {
                 expected_move = chess::uci::parseSan(display_board, expected_san.substr(0, expected_san.size()-1), cached_legal_moves);
            }

            if (expected_move != chess::Move::NO_MOVE) {
                float pulse = (sinf((float)ImGui::GetTime() * 6.0f) * 0.5f + 0.5f);
                int alpha = 60 + (int)(pulse * 100.0f);
                
                if (context->game.hint_level >= 1) {
                    int col, row;
                    BoardGeometry::GetVisualFromSquare(expected_move.from(), cached_cos, cached_sin, col, row);
                    ImVec2 pos = BoardGeometry::GetSquareScreenPos(board_top_left, col, row, SQUARE_SIZE, context->game.effects.GetShakeOffset());
                    draw_list->AddRectFilled(pos, ImVec2(pos.x + SQUARE_SIZE, pos.y + SQUARE_SIZE), IM_COL32(255, 255, 0, alpha)); 
                }
                if (context->game.hint_level >= 2) {
                    int col, row;
                    BoardGeometry::GetVisualFromSquare(expected_move.to(), cached_cos, cached_sin, col, row);
                    ImVec2 pos = BoardGeometry::GetSquareScreenPos(board_top_left, col, row, SQUARE_SIZE, context->game.effects.GetShakeOffset());
                    draw_list->AddRectFilled(pos, ImVec2(pos.x + SQUARE_SIZE, pos.y + SQUARE_SIZE), IM_COL32(0, 255, 0, alpha)); 
                }
            }
        }

        // Right-Click Arrow Logic
        if (board_hovered && right_clicked) {
            context->game.is_right_dragging = true;
            context->game.current_drawing_arrow.from = hovered_sq;
            context->game.current_drawing_arrow.to = hovered_sq;
        }
        if (context->game.is_right_dragging && right_dragging) {
            if (board_hovered) context->game.current_drawing_arrow.to = hovered_sq;
        }
        if (context->game.is_right_dragging && right_released) {
            context->game.is_right_dragging = false;
            Arrow new_arrow = context->game.current_drawing_arrow;
            if (board_hovered) new_arrow.to = hovered_sq; 
            auto it = std::find(context->game.arrows.begin(), context->game.arrows.end(), new_arrow);
            if (it != context->game.arrows.end()) context->game.arrows.erase(it);
            else context->game.arrows.push_back(new_arrow);
        }

        if (mouse_clicked && board_hovered) context->game.arrows.clear();

        bool interaction_allowed = !context->promo_state.active;

        if (interaction_allowed && mouse_clicked && hovered_sq != chess::Square::underlying::NO_SQ) {
            chess::Piece piece = display_board.at<chess::Piece>(hovered_sq);
            if (piece != chess::Piece::NONE && piece.color() == display_board.sideToMove()) {
                context->drag_state.is_dragging = true;
                context->drag_state.source_sq = hovered_sq;
                context->drag_state.piece_idx = static_cast<int>(piece.internal());
                int c, r; BoardGeometry::GetVisualFromSquare(hovered_sq, cached_cos, cached_sin, c, r);
                ImVec2 piece_visual_pos = BoardGeometry::GetSquareScreenPos(board_top_left, c, r, SQUARE_SIZE, context->game.effects.GetShakeOffset());
                context->drag_state.mouse_offset = ImVec2(piece_visual_pos.x - mouse_pos.x, piece_visual_pos.y - mouse_pos.y);
            }
        }

        if (mouse_released && context->drag_state.is_dragging) {
            if (hovered_sq != chess::Square::underlying::NO_SQ && hovered_sq != context->drag_state.source_sq) {
                bool is_promo = false;
                chess::Move m = context->GetLegalMove(display_board, context->drag_state.source_sq, hovered_sq, &is_promo);
                if (is_promo) {
                    context->promo_state.active = true; context->promo_state.from = context->drag_state.source_sq; context->promo_state.to = hovered_sq; context->promo_state.side = display_board.sideToMove();
                } else if (m != chess::Move::NO_MOVE) {
                    context->HandlePuzzleLogic(m);
                    display_board = chess::Board(context->game.start_fen);
                    auto line = context->game.get_current_line_indices();
                    for(int idx : line) display_board.makeMove(context->game.move_tree[idx].move);
                }
            }
            context->drag_state.is_dragging = false; context->drag_state.source_sq = chess::Square::underlying::NO_SQ;
        }

        // Animation Block
        float anim_x = 0, anim_y = 0; bool is_animating = false;
        if (context->move_anim.active) {
            Uint64 now = SDL_GetTicks();
            if (now < context->move_anim.start_time + context->move_anim.duration) {
                float t = (float)(now - context->move_anim.start_time) / (float)context->move_anim.duration;
                t = t * t * (3.0f - 2.0f * t);
                int from_c, from_r, to_c, to_r;
                BoardGeometry::GetVisualFromSquare(context->move_anim.from, cached_cos, cached_sin, from_c, from_r);
                BoardGeometry::GetVisualFromSquare(context->move_anim.to, cached_cos, cached_sin, to_c, to_r);
                ImVec2 start = BoardGeometry::GetSquareScreenPos(board_top_left, from_c, from_r, SQUARE_SIZE, context->game.effects.GetShakeOffset());
                ImVec2 end = BoardGeometry::GetSquareScreenPos(board_top_left, to_c, to_r, SQUARE_SIZE, context->game.effects.GetShakeOffset());
                anim_x = start.x + (end.x - start.x) * t; anim_y = start.y + (end.y - start.y) * t; is_animating = true;
                
                float duration_sec = std::max(0.001f, context->move_anim.duration / 1000.0f);
                ImVec2 velocity = ImVec2((end.x - start.x) / duration_sec, (end.y - start.y) / duration_sec);
                context->ghost_timer += ImGui::GetIO().DeltaTime;
                if (context->ghost_timer > 0.01f) {
                    if (context->move_anim.piece_idx != -1) {
                        context->game.effects.SpawnGhost(ImVec2(anim_x, anim_y), context->move_anim.piece_idx, velocity);
                        context->game.effects.SpawnTrail(ImVec2(anim_x + SQUARE_SIZE/2, anim_y + SQUARE_SIZE/2));
                    }
                    context->ghost_timer = 0.0f;
                }
            } else {
                if (context->move_anim.active && context->game.effects.enable_thud) {
                    int to_c, to_r; BoardGeometry::GetVisualFromSquare(context->move_anim.to, cached_cos, cached_sin, to_c, to_r);
                    ImVec2 end = BoardGeometry::GetSquareScreenPos(board_top_left, to_c, to_r, SQUARE_SIZE, context->game.effects.GetShakeOffset());
                    float intensity = 1.0f; int p_type = context->move_anim.piece_idx % 8;
                    if (p_type == 0) intensity = 0.7f; else if (p_type == 3) intensity = 1.5f; else if (p_type == 4) intensity = 1.8f; else if (p_type == 5) intensity = 2.0f; else intensity = 1.1f;
                    ImVec2 center = ImVec2(end.x + SQUARE_SIZE * 0.5f, end.y + SQUARE_SIZE * 0.5f);
                    
                    context->game.effects.SpawnFlash(end); 
                    context->game.effects.SpawnSquash(context->move_anim.to, intensity); 
                    context->game.effects.SpawnRipple(center, intensity);
                    
                    {
                        chess::Piece landed_piece = display_board.at<chess::Piece>(context->move_anim.to);
                        int landed_type = static_cast<int>(landed_piece.type());
                        bool is_white = (landed_piece.color() == chess::Color::WHITE);
                        context->game.effects.SpawnImpactWobble(context->move_anim.to, landed_type, is_white, intensity, context->move_anim.is_correct_puzzle_move);
                    }
                }
                context->move_anim.active = false; context->ghost_timer = 0.0f;
            }
        }

        // Piece Drawing Logic (Respect Blindfold)
        bool hide_pieces = context->blindfold_active && context->current_blindfold_timer <= 0.0f;

        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                chess::Square sq = BoardGeometry::GetSquareFromVisual(col, row, cached_cos, cached_sin);
                if (context->drag_state.is_dragging && context->drag_state.source_sq == sq) continue;
                if (is_animating && context->move_anim.to == sq) continue;

                chess::Piece piece = display_board.at<chess::Piece>(sq);
                if (piece != chess::Piece::NONE && !hide_pieces) {
                    ImVec2 pos = BoardGeometry::GetSquareScreenPos(board_top_left, col, row, SQUARE_SIZE, context->game.effects.GetShakeOffset());
                    int idx = static_cast<int>(piece.internal());
                    
                    ImVec2 scale = context->game.effects.GetPieceScale(sq);
                    bool is_white = (piece.color() == chess::Color::WHITE);
                    bool player_is_white = !context->game.is_flipped;
                    float depression = context->game.effects.GetSquareDepression(sq);
                    float offset_y = context->game.effects.GetWobbleOffset(row * 8 + col, false) + context->game.effects.GetEnemyShakeOffset(sq, is_white, player_is_white, false) + depression;
                    float offset_x = context->game.effects.GetWobbleOffset(row * 8 + col, true) + context->game.effects.GetEnemyShakeOffset(sq, is_white, player_is_white, true);

                    float tilt = 0.0f;
                    ImVec2 radial_tilt(0.0f, 0.0f);
                    if (context->game.effects.enable_physicality && !context->game.effects.disable_all_effects) {
                        float speed_x = (float)sin(ImGui::GetTime() * 3.0 + (double)(row*8+col));
                        tilt = speed_x * 0.05f * context->game.effects.phys_tilt_amount;
                        radial_tilt = context->game.effects.GetWaveTilt(sq);
                        float tilt_magnitude = sqrtf(radial_tilt.x * radial_tilt.x + radial_tilt.y * radial_tilt.y);
                        scale.y *= std::max(0.6f, 1.0f - tilt_magnitude * 0.6f); // foreshortening
                        if (context->drag_state.is_dragging && context->drag_state.source_sq == sq) {
                            tilt = 0.0f;
                            radial_tilt = ImVec2(0.0f, 0.0f);
                        }
                    }

                    float p_x = 0.0f;
                    float p_y = 0.0f;
                    float p_spread = 0.0f;

                    if (context->game.effects.enable_victory_dance && context->game.effects.victory_timer > 0.0 && is_white == player_is_white) {
                        float fade_t = (float)(context->game.effects.victory_timer / context->game.effects.victory_dance_duration);
                        float fade_mult = (fade_t > 0.2f) ? 1.0f : (fade_t / 0.2f); // Keep full intensity until the last 20%
                        float v_intensity = fade_mult * context->game.effects.victory_dance_intensity;

                        if (context->game.effects.victory_dance_type == 0) { // Classic Bounce & Wiggle
                            float v_time = (float)ImGui::GetTime() * 15.0f + (col + row) * 0.8f;
                            offset_y -= std::abs(sinf(v_time)) * 20.0f * v_intensity;
                            tilt += cosf(v_time) * 0.2f * v_intensity;
                        } else if (context->game.effects.victory_dance_type == 1) { // Cheer / Jumping Jack
                            float v_time = (float)ImGui::GetTime() * 12.0f + (col % 2) * 3.14159f; 
                            float bounce = std::abs(sinf(v_time));
                            offset_y -= bounce * 25.0f * v_intensity; 
                            p_spread = bounce * 15.0f * v_intensity;  
                        } else if (context->game.effects.victory_dance_type == 2) { // Swirl Parallax
                            float v_time = (float)ImGui::GetTime() * 10.0f;
                            p_x = cosf(v_time + col) * 15.0f * v_intensity;
                            p_y = sinf(v_time + col) * 15.0f * v_intensity;
                        } else if (context->game.effects.victory_dance_type == 3) { // Stadium Wave
                            float elapsed = (float)(context->game.effects.victory_dance_duration - context->game.effects.victory_timer);

                            // Make the wave travel across the board (col 0 to 7) over 1.5 seconds.
                            // Cycle repeats every 2.5 seconds.
                            float wave_duration = 1.5f; 
                            float wave_speed = wave_duration / 8.0f; 
                            float local_time = elapsed - (col * wave_speed); 

                            float cycle_time = fmodf(local_time, 2.5f);
                            float wave_pulse = 0.0f;

                            // A piece jumps and lands over 0.5 seconds
                            if (local_time > 0.0f && cycle_time < 0.5f) {
                                float t = cycle_time / 0.5f; 
                                wave_pulse = 4.0f * t * (1.0f - t); // Smooth parabola 0 -> 1 -> 0
                            }

                            offset_y -= wave_pulse * 40.0f * v_intensity; // Sharp jump
                            tilt += wave_pulse * ((col % 2 == 0) ? 0.2f : -0.2f) * v_intensity; // Alternate tilting
                            p_y = wave_pulse * 10.0f * v_intensity; // Slight stretch downwards
                        }
                    }

                    if (idx >= 0 && idx < 16 && assets->theme_manager.piece_textures[idx] != 0) {
                        // Apply squash scaling from the bottom of the piece (base), not the center
                        ImVec2 base_center(pos.x + SQUARE_SIZE * 0.5f + offset_x, pos.y + SQUARE_SIZE + offset_y);
                        ImVec2 half_sz(SQUARE_SIZE * 0.5f * scale.x, SQUARE_SIZE * 0.5f * scale.y);

                        ImVec2 center(base_center.x, base_center.y - half_sz.y);

                        ImVec2 p1 = ImVec2(-half_sz.x + p_x - p_spread + radial_tilt.x * 25.0f, -half_sz.y + p_y + radial_tilt.y * 25.0f);
                        ImVec2 p2 = ImVec2( half_sz.x + p_x + p_spread + radial_tilt.x * 25.0f, -half_sz.y + p_y + radial_tilt.y * 25.0f);
                        ImVec2 p3 = ImVec2( half_sz.x,  half_sz.y);
                        ImVec2 p4 = ImVec2(-half_sz.x,  half_sz.y);
                        float c = 1.0f; float s = 0.0f;
                        if (tilt != 0.0f && context->game.effects.enable_rotation) {
                            c = cosf(tilt); s = sinf(tilt);
                        }

                        auto Transform = [&](ImVec2 v) { return ImVec2(v.x*c - v.y*s + center.x, v.x*s + v.y*c + center.y); };

                        // Dynamic Drop Shadow (lifted pieces)
                        if (depression < -0.1f) {
                            float shadow_alpha = std::clamp(1.0f - (std::abs(depression) / 30.0f), 0.0f, 1.0f);
                            float sx = std::abs(depression) * 0.1f;
                            float sy = std::abs(depression) * 0.3f;
                            draw_list->AddImageQuad((ImTextureID)(intptr_t)assets->theme_manager.piece_textures[idx],
                                Transform(ImVec2(p1.x + sx, p1.y + sy)), Transform(ImVec2(p2.x + sx, p2.y + sy)),
                                Transform(ImVec2(p3.x + sx, p3.y + sy)), Transform(ImVec2(p4.x + sx, p4.y + sy)),
                                ImVec2(0,0), ImVec2(1,0), ImVec2(1,1), ImVec2(0,1),
                                IM_COL32(0, 0, 0, (int)(100 * shadow_alpha)));
                        }

                        // Chromatic Aberration on strong thuds
                        float squash_timer = 0.0f;
                        float squash_intensity = 0.0f;
                        for (const auto& s : context->game.effects.squashes) {
                            if (s.sq == sq) { squash_timer = s.timer; squash_intensity = s.intensity; break; }
                        }
                        
                        if (squash_timer > 0.0f && squash_intensity > 1.2f) {
                            float t = 1.0f - (squash_timer / 0.15f);
                            if (t < 0.6f) { // Only visible for the first half of the squash
                                float ca_fade = 1.0f - (t / 0.6f);
                                float shift_amt = (5.0f + squash_intensity * 3.0f) * ca_fade;
                                
                                ImVec2 p1_r = ImVec2(p1.x - shift_amt, p1.y); ImVec2 p2_r = ImVec2(p2.x - shift_amt, p2.y);
                                ImVec2 p3_r = ImVec2(p3.x - shift_amt, p3.y); ImVec2 p4_r = ImVec2(p4.x - shift_amt, p4.y);
                                
                                ImVec2 p1_c = ImVec2(p1.x + shift_amt, p1.y); ImVec2 p2_c = ImVec2(p2.x + shift_amt, p2.y);
                                ImVec2 p3_c = ImVec2(p3.x + shift_amt, p3.y); ImVec2 p4_c = ImVec2(p4.x + shift_amt, p4.y);
                                
                                draw_list->AddImageQuad((ImTextureID)(intptr_t)assets->theme_manager.piece_textures[idx], Transform(p1_r), Transform(p2_r), Transform(p3_r), Transform(p4_r), ImVec2(0,0), ImVec2(1,0), ImVec2(1,1), ImVec2(0,1), IM_COL32(255, 0, 100, (int)(150 * ca_fade)));
                                draw_list->AddImageQuad((ImTextureID)(intptr_t)assets->theme_manager.piece_textures[idx], Transform(p1_c), Transform(p2_c), Transform(p3_c), Transform(p4_c), ImVec2(0,0), ImVec2(1,0), ImVec2(1,1), ImVec2(0,1), IM_COL32(0, 255, 255, (int)(150 * ca_fade)));
                            }
                        }

                        draw_list->AddImageQuad((ImTextureID)(intptr_t)assets->theme_manager.piece_textures[idx], Transform(p1), Transform(p2), Transform(p3), Transform(p4));
                    }
                }
            }
        }

        if (is_animating && !hide_pieces) {
            chess::Piece piece = display_board.at<chess::Piece>(context->move_anim.to); 
            if (piece != chess::Piece::NONE) {
                int idx = static_cast<int>(piece.internal());
                if (idx >= 0 && idx < 16 && assets->theme_manager.piece_textures[idx] != 0) {
                    draw_list->AddImage((ImTextureID)(intptr_t)assets->theme_manager.piece_textures[idx], ImVec2(anim_x, anim_y), ImVec2(anim_x + SQUARE_SIZE, anim_y + SQUARE_SIZE));
                }
            }
        }

        if (context->drag_state.is_dragging && context->drag_state.piece_idx != -1 && !hide_pieces) {
             if (hovered_sq != chess::Square::underlying::NO_SQ) {
                 int h_c, h_r; BoardGeometry::GetVisualFromSquare(hovered_sq, cached_cos, cached_sin, h_c, h_r);
                 ImVec2 pos = BoardGeometry::GetSquareScreenPos(board_top_left, h_c, h_r, SQUARE_SIZE, context->game.effects.GetShakeOffset());
                 draw_list->AddRect(pos, ImVec2(pos.x + SQUARE_SIZE, pos.y + SQUARE_SIZE), IM_COL32(255, 255, 255, 200), 0.0f, 0, 4.0f);
            }
            float x = mouse_pos.x + context->drag_state.mouse_offset.x, y = mouse_pos.y + context->drag_state.mouse_offset.y;
            if (context->drag_state.piece_idx >= 0 && context->drag_state.piece_idx < 16 && assets->theme_manager.piece_textures[context->drag_state.piece_idx] != 0) {
                draw_list->AddImage((ImTextureID)(intptr_t)assets->theme_manager.piece_textures[context->drag_state.piece_idx], ImVec2(x, y), ImVec2(x + SQUARE_SIZE, y + SQUARE_SIZE));
            }
        }

        // Blindfold Memorization Overlay
        if (context->blindfold_active && context->current_blindfold_timer > 0.0f) {
            char timer_text[16]; snprintf(timer_text, 16, "%.1fs", context->current_blindfold_timer);
            ImVec2 txt_sz = ImGui::CalcTextSize(timer_text);
            draw_list->AddRectFilled(ImVec2(board_top_left.x + 5, board_top_left.y + 5), ImVec2(board_top_left.x + 15 + txt_sz.x, board_top_left.y + 15 + txt_sz.y), IM_COL32(0, 0, 0, 180), 4.0f);
            draw_list->AddText(ImVec2(board_top_left.x + 10, board_top_left.y + 10), IM_COL32(255, 255, 0, 255), timer_text);
        }

        // Draw Arrows
        ImU32 arrow_col = IM_COL32(0, 255, 0, 80); 
        for (const auto& arrow : context->game.arrows) {
            int c1, r1, c2, r2;
            BoardGeometry::GetVisualFromSquare(arrow.from, cached_cos, cached_sin, c1, r1);
            BoardGeometry::GetVisualFromSquare(arrow.to, cached_cos, cached_sin, c2, r2);
            ImVec2 start = BoardGeometry::GetSquareScreenPos(board_top_left, c1, r1, SQUARE_SIZE, context->game.effects.GetShakeOffset());
            ImVec2 end = BoardGeometry::GetSquareScreenPos(board_top_left, c2, r2, SQUARE_SIZE, context->game.effects.GetShakeOffset());
            start.x += SQUARE_SIZE * 0.5f; start.y += SQUARE_SIZE * 0.5f; end.x += SQUARE_SIZE * 0.5f; end.y += SQUARE_SIZE * 0.5f;
            if (arrow.from == arrow.to) draw_list->AddCircle(start, SQUARE_SIZE * 0.42f, arrow_col, 0, 6.0f);
            else DrawArrow(draw_list, start, end, arrow_col);
        }

        auto DrawEngineBestMove = [&](EngineManager& eng, ImU32 col) {
            if (!eng.IsRunning()) return;
            auto info = eng.GetInfo();
            if (!info.best_move.empty()) {
                chess::Move best = chess::uci::uciToMove(display_board, info.best_move);
                if (best != chess::Move::NO_MOVE) {
                    int c1, r1, c2, r2;
                    BoardGeometry::GetVisualFromSquare(best.from(), cached_cos, cached_sin, c1, r1);
                    BoardGeometry::GetVisualFromSquare(best.to(), cached_cos, cached_sin, c2, r2);
                    
                    ImVec2 start = BoardGeometry::GetSquareScreenPos(board_top_left, c1, r1, SQUARE_SIZE, context->game.effects.GetShakeOffset());
                    ImVec2 end = BoardGeometry::GetSquareScreenPos(board_top_left, c2, r2, SQUARE_SIZE, context->game.effects.GetShakeOffset());
                    
                    start.x += SQUARE_SIZE * 0.5f; start.y += SQUARE_SIZE * 0.5f;
                    end.x += SQUARE_SIZE * 0.5f; end.y += SQUARE_SIZE * 0.5f;
                    
                    DrawArrow(draw_list, start, end, col);
                }
            }
        };
        
        if (context->game.sf_active) DrawEngineBestMove(context->game.sf_engine, IM_COL32(0, 0, 255, 100)); 
        if (context->game.lc0_active) DrawEngineBestMove(context->game.lc0_engine, IM_COL32(255, 165, 0, 100)); 

        if (context->game.is_right_dragging) {
            int c1, r1, c2, r2;
            BoardGeometry::GetVisualFromSquare(context->game.current_drawing_arrow.from, cached_cos, cached_sin, c1, r1);
            BoardGeometry::GetVisualFromSquare(context->game.current_drawing_arrow.to, cached_cos, cached_sin, c2, r2);
            ImVec2 start = BoardGeometry::GetSquareScreenPos(board_top_left, c1, r1, SQUARE_SIZE, context->game.effects.GetShakeOffset());
            ImVec2 end = BoardGeometry::GetSquareScreenPos(board_top_left, c2, r2, SQUARE_SIZE, context->game.effects.GetShakeOffset());
            start.x += SQUARE_SIZE * 0.5f; start.y += SQUARE_SIZE * 0.5f; end.x += SQUARE_SIZE * 0.5f; end.y += SQUARE_SIZE * 0.5f;
            DrawArrow(draw_list, start, end, arrow_col);
        }

        // Promotion Menu
        if (context->promo_state.active) {
            int dst_c, dst_r; BoardGeometry::GetVisualFromSquare(context->promo_state.to, cached_cos, cached_sin, dst_c, dst_r);
            ImVec2 promo_pos = BoardGeometry::GetSquareScreenPos(board_top_left, dst_c, dst_r, SQUARE_SIZE, context->game.effects.GetShakeOffset());
            ImGui::SetNextWindowPos(promo_pos); ImGui::OpenPopup("PromotionMenu");
            if (ImGui::BeginPopup("PromotionMenu", ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
                const chess::PieceType types[] = { chess::PieceType::QUEEN, chess::PieceType::ROOK, chess::PieceType::BISHOP, chess::PieceType::KNIGHT };
                for (chess::PieceType pt : types) {
                    chess::Piece p(context->promo_state.side, pt); int idx = static_cast<int>(p.internal());
                    ImGui::PushID((int)pt);
                    if (ImGui::ImageButton((std::to_string((int)pt)).c_str(), (ImTextureID)(intptr_t)assets->theme_manager.piece_textures[idx], ImVec2(SQUARE_SIZE * 0.8f, SQUARE_SIZE * 0.8f))) {
                        chess::Move m = chess::Move::make<chess::Move::PROMOTION>(context->promo_state.from, context->promo_state.to, pt);
                        context->promo_state.active = false; ImGui::CloseCurrentPopup(); context->HandlePuzzleLogic(m);
                    }
                    ImGui::PopID(); ImGui::SameLine();
                }
                ImGui::EndPopup();
            } else {
                if (!ImGui::IsPopupOpen("PromotionMenu")) context->promo_state.active = false;
            }
        }
    }

    void DrawMainMenuBar() {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Import PGN...", "Ctrl+O")) {
                    std::string path = OpenFileDialog("PGN Files (*.pgn)\0*.pgn\0All Files (*.*)\0*.*\0", "Select PGN File");
                    if (!path.empty()) { if (context->ImportPGN(path)) assets->PlaySound(assets->snd_confirm); else assets->PlaySound(assets->snd_wrong); }
                }
                if (ImGui::MenuItem("Import from Clipboard", "Ctrl+V")) {
                    const char* clip = ImGui::GetClipboardText();
                    if (clip && clip[0] != '\0') { if (context->ImportPGNFromText(std::string(clip))) assets->PlaySound(assets->snd_confirm); else assets->PlaySound(assets->snd_wrong); } 
                    else assets->PlaySound(assets->snd_wrong);
                }
                if (ImGui::MenuItem("Save Settings", "Ctrl+S")) { SaveSettings(); assets->PlaySound(assets->snd_confirm); }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) { SDL_Event quit; quit.type = SDL_EVENT_QUIT; SDL_PushEvent(&quit); }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Game")) {
                if (ImGui::MenuItem("Reset Game/Puzzle", "R")) {
                    if (context->puzzle_manager.GetCurrentPuzzle()) context->StartPuzzle(context->puzzle_manager.GetCurrentPuzzle());
                    else context->game.reset();
                    assets->PlaySound(assets->snd_confirm);
                }
                
                if (ImGui::MenuItem("Load from Clipboard", "Ctrl+Shift+V")) {
                    const char* clip = SDL_GetClipboardText();
                    if (clip) {
                        std::string clip_str = clip;
                        SDL_free((void*)clip);
                        
                        size_t pos = clip_str.find_last_of('/');
                        if (pos != std::string::npos) clip_str = clip_str.substr(pos + 1);
                        
                        if (!clip_str.empty()) {
                            if (context->puzzle_manager.LoadPuzzleById(clip_str)) {
                                context->current_mode = GameContext::PlayMode::FREE_PLAY;
                                context->game.timer_running = false;
                                context->StartPuzzle(context->puzzle_manager.GetCurrentPuzzle());
                                assets->PlaySound(assets->snd_startup);
                            } else {
                                context->puzzle_status = "Puzzle not found: " + clip_str;
                                assets->PlaySound(assets->snd_wrong);
                            }
                        }
                    }
                }

                ImGui::Separator();
                if (ImGui::BeginMenu("Auto Solve")) {
                    if (ImGui::MenuItem("Enabled", nullptr, &context->auto_solve_active)) {
                        if (context->auto_solve_active) {
                            context->auto_solve_state = GameContext::AutoSolveState::WAITING;
                            context->auto_solve_timer = 1.0f;
                        }
                    }
                    
                    ImGui::Separator();
                    ImGui::TextDisabled("Speed:");
                    if (ImGui::RadioButton("Slow", context->auto_solve_speed == GameContext::AutoSolveSpeed::SLOW)) 
                        context->auto_solve_speed = GameContext::AutoSolveSpeed::SLOW;
                    if (ImGui::RadioButton("Normal", context->auto_solve_speed == GameContext::AutoSolveSpeed::NORMAL)) 
                        context->auto_solve_speed = GameContext::AutoSolveSpeed::NORMAL;
                    if (ImGui::RadioButton("Fast", context->auto_solve_speed == GameContext::AutoSolveSpeed::FAST)) 
                        context->auto_solve_speed = GameContext::AutoSolveSpeed::FAST;
                    if (ImGui::RadioButton("Custom", context->auto_solve_speed == GameContext::AutoSolveSpeed::CUSTOM))
                        context->auto_solve_speed = GameContext::AutoSolveSpeed::CUSTOM;
                    
                    if (context->auto_solve_speed == GameContext::AutoSolveSpeed::CUSTOM) {
                        ImGui::Indent();
                        ImGui::SetNextItemWidth(120);
                        ImGui::SliderFloat("Delay (s)", &context->auto_solve_custom_delay, 1.0f, 60.0f, "%.1fs");
                        ImGui::Unindent();
                    }
                    
                    ImGui::Separator();
                    ImGui::MenuItem("Show Hint", nullptr, &context->auto_solve_show_hint);
                    
                    if (ImGui::MenuItem("Update DB (Ranked)", nullptr, &context->auto_solve_update_db)) {
                        /* Optional Sound */
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("WARNING: Enabling this will affect your Rating and Stats!");
                    
                    ImGui::EndMenu();
                    }

                    if (ImGui::BeginMenu("Auto Play")) {
                        if (ImGui::MenuItem("Enabled", nullptr, &context->game.auto_play_active)) {
                            context->game.auto_play_timer = 0.0f;
                            if (context->game.auto_play_active) {
                                if (context->game.auto_play_white == GameState::AutoPlayPlayer::STOCKFISH || context->game.auto_play_black == GameState::AutoPlayPlayer::STOCKFISH) {
                                    context->game.sf_active = true;
                                }
                                if (context->game.auto_play_white == GameState::AutoPlayPlayer::LC0 || context->game.auto_play_black == GameState::AutoPlayPlayer::LC0) {
                                    context->game.lc0_active = true;
                                }
                                auto op = context->game.GetSelectedOpening();
                                context->game.reset(op.second);
                            }
                        }
                        ImGui::Separator();

                        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 4.0f));
                        if (ImGui::BeginTable("AutoPlaySettings", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit)) {
                            ImGui::TableSetupColumn("Setting", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                            ImGui::TableSetupColumn("White", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                            ImGui::TableSetupColumn("Black", ImGuiTableColumnFlags_WidthFixed, 120.0f);

                            // Row 1: Headers
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0); ImGui::Text(" ");
                            ImGui::TableSetColumnIndex(1); ImGui::Text("White"); ImGui::Separator();
                            ImGui::TableSetColumnIndex(2); ImGui::Text("Black"); ImGui::Separator();

                            const char* player_types[] = { "Human", "Stockfish", "Lc0" };

                            // Row 2: Player
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0); ImGui::AlignTextToFramePadding(); ImGui::Text("Player");
                            ImGui::TableSetColumnIndex(1); ImGui::PushID("W_Player"); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::Combo("##p", (int*)&context->game.auto_play_white, player_types, 3); ImGui::PopID();
                            ImGui::TableSetColumnIndex(2); ImGui::PushID("B_Player"); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::Combo("##p", (int*)&context->game.auto_play_black, player_types, 3); ImGui::PopID();

                            // Row 3: Use Timer
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0); ImGui::AlignTextToFramePadding(); ImGui::Text("Use Timer");
                            ImGui::TableSetColumnIndex(1); ImGui::PushID("W_Timer"); 
                            if (context->game.auto_play_white != GameState::AutoPlayPlayer::HUMAN) { ImGui::Checkbox("##t", &context->game.auto_play_white_use_time); } ImGui::PopID();
                            ImGui::TableSetColumnIndex(2); ImGui::PushID("B_Timer"); 
                            if (context->game.auto_play_black != GameState::AutoPlayPlayer::HUMAN) { ImGui::Checkbox("##t", &context->game.auto_play_black_use_time); } ImGui::PopID();

                            // Row 4: Time (s)
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0); ImGui::AlignTextToFramePadding(); ImGui::Text("Time (s)");
                            ImGui::TableSetColumnIndex(1); ImGui::PushID("W_TimeS");
                            if (context->game.auto_play_white != GameState::AutoPlayPlayer::HUMAN && context->game.auto_play_white_use_time) {
                                ImGui::SetNextItemWidth(-FLT_MIN); ImGui::SliderFloat("##ts", &context->game.auto_play_white_move_time, 0.1f, 60.0f, "%.1f");
                            } ImGui::PopID();
                            ImGui::TableSetColumnIndex(2); ImGui::PushID("B_TimeS");
                            if (context->game.auto_play_black != GameState::AutoPlayPlayer::HUMAN && context->game.auto_play_black_use_time) {
                                ImGui::SetNextItemWidth(-FLT_MIN); ImGui::SliderFloat("##ts", &context->game.auto_play_black_move_time, 0.1f, 60.0f, "%.1f");
                            } ImGui::PopID();

                            // Row 5: Target Depth/Nodes
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0); ImGui::AlignTextToFramePadding(); ImGui::Text("Limit");
                            ImGui::TableSetColumnIndex(1); ImGui::PushID("W_Target");
                            if (context->game.auto_play_white != GameState::AutoPlayPlayer::HUMAN) {
                                ImGui::SetNextItemWidth(-FLT_MIN);
                                if (context->game.auto_play_white == GameState::AutoPlayPlayer::STOCKFISH) ImGui::SliderInt("##td", &context->game.auto_play_white_depth, 1, 40);
                                else ImGui::SliderInt("##tn", &context->game.auto_play_white_nodes, 10, 20000);
                            } ImGui::PopID();
                            ImGui::TableSetColumnIndex(2); ImGui::PushID("B_Target");
                            if (context->game.auto_play_black != GameState::AutoPlayPlayer::HUMAN) {
                                ImGui::SetNextItemWidth(-FLT_MIN);
                                if (context->game.auto_play_black == GameState::AutoPlayPlayer::STOCKFISH) ImGui::SliderInt("##td", &context->game.auto_play_black_depth, 1, 40);
                                else ImGui::SliderInt("##tn", &context->game.auto_play_black_nodes, 10, 20000);
                            } ImGui::PopID();

                            ImGui::EndTable();
                        }
                        ImGui::PopStyleVar();

                        if (context->game.auto_play_use_separate_instances && ImGui::TreeNode("Advanced Engine Settings")) {
                            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 4.0f));
                            if (ImGui::BeginTable("AutoPlayAdvanced", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit)) {
                                ImGui::TableSetupColumn("White Config", ImGuiTableColumnFlags_WidthFixed, 140.0f);
                                ImGui::TableSetupColumn("Black Config", ImGuiTableColumnFlags_WidthFixed, 140.0f);
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0); ImGui::TextColored(ImVec4(0.7f,0.7f,0.7f,1.0f), "White Specific"); ImGui::Separator();
                                ImGui::TableSetColumnIndex(1); ImGui::TextColored(ImVec4(0.7f,0.7f,0.7f,1.0f), "Black Specific"); ImGui::Separator();

                                auto RenderEngineConfig = [&](GameState::AutoPlayPlayer player, EngineManager& sf, EngineManager& lc0, const char* id) {
                                    ImGui::PushID(id);
                                    if (player == GameState::AutoPlayPlayer::STOCKFISH) {
                                        bool changed = false;
                                        ImGui::SetNextItemWidth(-FLT_MIN);
                                        if (ImGui::SliderInt("##th", &sf.threads, 1, std::thread::hardware_concurrency(), "Threads: %d")) changed = true;
                                        ImGui::SetNextItemWidth(-FLT_MIN);
                                        if (ImGui::InputInt("##hash", &sf.hash, 16, 128)) changed = true;
                                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Hash (MB)");
                                        if (changed) sf.ApplyOptions();
                                    } else if (player == GameState::AutoPlayPlayer::LC0) {
                                        bool changed = false;
                                        ImGui::SetNextItemWidth(-FLT_MIN);
                                        if (ImGui::InputInt("##nnc", &lc0.nn_cache_size, 100000, 500000)) changed = true;
                                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("NN Cache Size");
                                        ImGui::SetNextItemWidth(-FLT_MIN);
                                        if (ImGui::InputInt("##mb", &lc0.minibatch_size, 64, 128)) changed = true;
                                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Minibatch Size");
                                        if (changed) lc0.ApplyOptions();
                                    } else {
                                        ImGui::TextDisabled("N/A (Human)");
                                    }
                                    ImGui::PopID();
                                };

                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                RenderEngineConfig(context->game.auto_play_white, context->game.sf_engine_white, context->game.lc0_engine_white, "W_Adv");
                                ImGui::TableSetColumnIndex(1);
                                RenderEngineConfig(context->game.auto_play_black, context->game.sf_engine_black, context->game.lc0_engine_black, "B_Adv");

                                ImGui::EndTable();
                            }
                            ImGui::PopStyleVar();
                            ImGui::TreePop();
                        }

                        ImGui::Separator();
                        ImGui::Checkbox("Auto-Restart Game", &context->game.auto_play_restart);

                        const auto& openings = context->game.GetAvailableOpenings();
                        if (ImGui::BeginCombo("Opening", context->game.auto_play_opening_idx == 0 ? "Random (From List)" : openings[context->game.auto_play_opening_idx - 1].name.c_str())) {
                            bool is_selected = (context->game.auto_play_opening_idx == 0);
                            if (ImGui::Selectable("Random (From List)", is_selected)) {
                                if (context->game.auto_play_opening_idx != 0) {
                                    context->game.auto_play_opening_idx = 0;
                                    auto op = context->game.GetSelectedOpening();
                                    context->game.reset(op.second);
                                }
                            }
                            if (is_selected) ImGui::SetItemDefaultFocus();

                            for (int i = 0; i < openings.size(); ++i) {
                                is_selected = (context->game.auto_play_opening_idx == i + 1);
                                if (ImGui::Selectable(openings[i].name.c_str(), is_selected)) {
                                    if (context->game.auto_play_opening_idx != i + 1) {
                                        context->game.auto_play_opening_idx = i + 1;
                                        auto op = context->game.GetSelectedOpening();
                                        context->game.reset(op.second);
                                    }
                                }
                                if (is_selected) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select a specific starting position or randomize from standard openings.");

                        ImGui::Checkbox("Auto-Save PGNs", &context->game.auto_play_save_pgn);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Saves finished matches to Assets/user_data/Autoplay/");
                        
                        ImGui::Checkbox("Use Separate Instances", &context->game.auto_play_use_separate_instances);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Runs completely separate engine processes for White and Black for true isolation.");

                        ImGui::TextDisabled("Score: W: %d | B: %d | D: %d", context->game.ap_wins_white, context->game.ap_wins_black, context->game.ap_draws);
                        if (ImGui::Button("Reset Score")) {
                            context->game.ap_wins_white = 0;
                            context->game.ap_wins_black = 0;
                            context->game.ap_draws = 0;
                        }

                        ImGui::EndMenu();
                    }                    if (ImGui::BeginMenu("Blindfold Mode")) {                    if (ImGui::MenuItem("Enabled", "B", &context->blindfold_active)) {
                        if (context->blindfold_active) context->current_blindfold_timer = context->blindfold_use_timer ? context->blindfold_delay : 0.0f;
                        assets->PlaySound(assets->snd_select);
                    }
                    ImGui::Separator();
                    ImGui::MenuItem("Use Memorization Timer", nullptr, &context->blindfold_use_timer);
                    if (context->blindfold_use_timer) {
                        ImGui::Indent();
                        ImGui::SetNextItemWidth(120);
                        ImGui::SliderFloat("Visible (s)", &context->blindfold_delay, 1.0f, 15.0f, "%.1fs");
                        ImGui::Unindent();
                    }
                    ImGui::EndMenu();
                }
                
                ImGui::Separator();
                if (ImGui::MenuItem("Play vs Engine", nullptr, &context->play_vs_engine)) {
                    if (context->play_vs_engine) {
                        context->current_mode = GameContext::PlayMode::FREE_PLAY;
                        context->game.timer_running = false;
                        context->game.sf_active = true;
                        context->game.UpdateEngines(true);
                        assets->PlaySound(assets->snd_confirm);
                    } else {
                        assets->PlaySound(assets->snd_select);
                    }
                }
                
                ImGui::Separator();
                if (ImGui::MenuItem("Keyboard Move Input", nullptr, &context->enable_keyboard_input)) {
                    assets->PlaySound(assets->snd_select);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enables typing SAN (e.g. Nf3) to make moves directly. Disables keyboard shortcuts while active.");
                
                ImGui::Separator();
                if (ImGui::BeginMenu("Play Modes")) {
                    ImGui::TextColored(ImVec4(0, 1, 1, 1), "Arcade Settings");
                    ImGui::PushItemWidth(150);
                    if (ImGui::BeginCombo("Theme", context->puzzle_manager.current_theme_filter.c_str())) {
                        for (const auto& theme : context->puzzle_manager.available_themes) {
                            if (ImGui::Selectable(theme.c_str(), context->puzzle_manager.current_theme_filter == theme)) {
                                context->puzzle_manager.current_theme_filter = theme;
                                context->puzzle_manager.multi_theme_filter.clear();
                                assets->PlaySound(assets->snd_select);
                            }
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::PopItemWidth();
                    ImGui::Separator();

                    if (ImGui::MenuItem("Free Play", nullptr, context->current_mode == GameContext::PlayMode::FREE_PLAY)) {
                        context->current_mode = GameContext::PlayMode::FREE_PLAY;
                        context->mode_game_over = false;
                        context->session_pb_broken = false;
                        context->StartPuzzle(context->puzzle_manager.GetRandomPuzzle());
                        assets->PlaySound(assets->snd_select);
                    }
                    if (ImGui::MenuItem("Puzzle Storm", nullptr, context->current_mode == GameContext::PlayMode::STORM)) {
                        context->current_mode = GameContext::PlayMode::STORM;
                        context->storm_timer = 180.0f;
                        context->storm_score = 0;
                        context->session_max_combo = 0;
                        context->mode_game_over = false;
                        context->session_pb_broken = false;
                        context->puzzle_manager.session_used_ids.clear();
                        context->StartPuzzle(context->puzzle_manager.GetProgressivePuzzle(0));
                        assets->PlaySound(assets->snd_select);
                    }
                    if (ImGui::MenuItem("Puzzle Streak", nullptr, context->current_mode == GameContext::PlayMode::STREAK)) {
                        context->current_mode = GameContext::PlayMode::STREAK;
                        context->streak_score = 0;
                        context->streak_lives = 3;
                        context->session_max_combo = 0;
                        context->mode_game_over = false;
                        context->session_pb_broken = false;
                        context->puzzle_manager.session_used_ids.clear();
                        context->StartPuzzle(context->puzzle_manager.GetProgressivePuzzle(0));
                        assets->PlaySound(assets->snd_select);
                    }
                    ImGui::EndMenu();
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Flip Board", "F", &context->game.is_flipped)) assets->PlaySound(assets->snd_select);
                if (ImGui::MenuItem("Auto-Next Puzzle", "A", &context->game.auto_next)) assets->PlaySound(assets->snd_select);
                ImGui::Separator();
                if (ImGui::MenuItem("Undo Delete", nullptr, false, !context->puzzle_manager.undo_stack.empty())) { context->puzzle_manager.UndoDelete(); assets->PlaySound(assets->snd_confirm); }
                if (ImGui::MenuItem("Redo Delete", nullptr, false, !context->puzzle_manager.redo_stack.empty())) { context->puzzle_manager.RedoDelete(); assets->PlaySound(assets->snd_confirm); }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Dashboard", nullptr, &settings.state.show_controls);
                ImGui::MenuItem("Move List", nullptr, &settings.state.show_move_list); 
                ImGui::MenuItem("Batch Analysis", nullptr, &settings.state.show_batch_dashboard);
                ImGui::MenuItem("AI Coach", nullptr, &settings.state.show_ai_coach);
                ImGui::MenuItem("History", nullptr, &settings.state.show_history);
                ImGui::MenuItem("Engines Tab", "E", &settings.state.show_engine_tab);
                ImGui::MenuItem("Statistics Window", nullptr, &settings.state.show_stats);
                ImGui::MenuItem("Evaluation Chart", nullptr, &settings.state.show_eval_chart);
                ImGui::MenuItem("Show Coordinates", nullptr, &settings.state.show_coordinates);
                ImGui::MenuItem("Performance Analytics", nullptr, &settings.state.show_performance_analytics);
                ImGui::Separator();
                ImGui::MenuItem("Show Title Bars", "Ctrl+T", &show_title_bars);
                ImGui::MenuItem("Focus Mode", "Z", &context->game.focus_mode);
                if (ImGui::MenuItem("Reset Default Layout")) reset_layout = true;
                ImGui::Separator();
                if (ImGui::BeginMenu("Overlays")) {
                    if (ImGui::MenuItem("Show Timer", "T", &settings.state.show_timer)) assets->PlaySound(assets->snd_select);
                    if (ImGui::MenuItem("Show Side to Move", nullptr, &settings.state.show_side_to_move)) assets->PlaySound(assets->snd_select);
                    if (ImGui::MenuItem("Show System Usage", nullptr, &settings.state.show_system_usage)) { if (settings.state.show_system_usage) context->sys_monitor.Init(); assets->PlaySound(assets->snd_select); }
                    if (ImGui::MenuItem("Show Dashboard Engine Stats", nullptr, &settings.state.show_dashboard_engine_stats)) assets->PlaySound(assets->snd_select);
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Effects")) {
                ImGui::MenuItem("Disable All Effects", nullptr, &context->game.effects.disable_all_effects);
                ImGui::Separator();

                if (ImGui::BeginMenu("Board & Environment")) {
                    if (ImGui::BeginMenu("Starfield")) {
                        ImGui::MenuItem("Enabled", nullptr, &context->game.effects.enable_starfield);
                        ImGui::Separator();
                        ImGui::PushItemWidth(120);
                        ImGui::SliderFloat("Warp Speed Multi", &context->game.effects.starfield_speed, 0.1f, 5.0f, "%.1f");
                        ImGui::SliderFloat("Trail Size", &context->game.effects.starfield_trail_size, 0.1f, 5.0f, "%.1f");
                        ImGui::PopItemWidth(); ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Square Colors")) {
                        ImGui::MenuItem("Enabled", nullptr, &context->game.effects.enable_square_colors);
                        ImGui::Separator();
                        ImGui::PushItemWidth(120);

                        float tint[3] = { context->game.effects.board_tint_r, context->game.effects.board_tint_g, context->game.effects.board_tint_b };
                        if (ImGui::ColorEdit3("Board Tint", tint)) {
                            context->game.effects.board_tint_r = tint[0];
                            context->game.effects.board_tint_g = tint[1];
                            context->game.effects.board_tint_b = tint[2];
                        }

                        ImGui::SliderFloat("Hue Shift", &context->game.effects.board_hue_shift, 0.0f, 1.0f, "%.2f");
                        ImGui::SliderFloat("Brightness", &context->game.effects.board_brightness, 0.0f, 2.0f, "%.2f");

                        float edge[3] = { context->game.effects.edge_color_r, context->game.effects.edge_color_g, context->game.effects.edge_color_b };
                        if (ImGui::ColorEdit3("3D Edge Color", edge)) {
                            context->game.effects.edge_color_r = edge[0];
                            context->game.effects.edge_color_g = edge[1];
                            context->game.effects.edge_color_b = edge[2];
                        }

                        ImGui::Separator();
                        if (ImGui::Button("Reset Defaults", ImVec2(-1, 0))) {
                            context->game.effects.enable_square_colors = false;
                            context->game.effects.board_tint_r = 1.0f;
                            context->game.effects.board_tint_g = 1.0f;
                            context->game.effects.board_tint_b = 1.0f;
                            context->game.effects.board_hue_shift = 0.0f;
                            context->game.effects.board_brightness = 1.0f;
                            context->game.effects.edge_color_r = 0.15f;
                            context->game.effects.edge_color_g = 0.13f;
                            context->game.effects.edge_color_b = 0.11f;
                        }

                        ImGui::PopItemWidth(); ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Piece Dynamics")) {
                    if (ImGui::BeginMenu("Physicality & Breathing")) {
                        ImGui::MenuItem("Enabled", nullptr, &context->game.effects.enable_physicality);
                        ImGui::Separator();
                        ImGui::PushItemWidth(120);
                        ImGui::SliderFloat("Bounce Strength", &context->game.effects.phys_bounce_strength, 0.0f, 2.0f, "%.1f");
                        ImGui::SliderFloat("Tilt Amount", &context->game.effects.phys_tilt_amount, 0.0f, 2.0f, "%.1f");
                        ImGui::PopItemWidth(); ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Movement Trails")) {
                        ImGui::MenuItem("Enabled", nullptr, &context->game.effects.enable_trail);
                        ImGui::Separator();
                        ImGui::PushItemWidth(120);

                        const char* styles[] = { "Sparks", "Cyber", "Ice", "Water", "Fire" };
                        int current_style = (int)context->game.effects.current_trail_style;
                        if (ImGui::BeginCombo("Style", styles[current_style])) {
                            for (int n = 0; n < 5; n++) if (ImGui::Selectable(styles[n], current_style == n)) context->game.effects.current_trail_style = (EffectsManager::TrailStyle)n;
                            ImGui::EndCombo();
                        }

                        const char* modes[] = { "Echo", "Light Streak", "Particles", "Blur" };
                        int current_mode = (int)context->game.effects.current_trail_mode;
                        if (ImGui::BeginCombo("Mode", modes[current_mode])) {
                            for (int n = 0; n < 4; n++) if (ImGui::Selectable(modes[n], current_mode == n)) context->game.effects.current_trail_mode = (EffectsManager::TrailMode)n;
                            ImGui::EndCombo();
                        }

                        ImGui::SliderFloat("Duration", &context->game.effects.trail_life, 0.1f, 2.0f, "%.1fs");
                        ImGui::SliderFloat("Speed", &context->game.effects.trail_speed_scale, 0.1f, 5.0f, "%.1f");
                        ImGui::SliderFloat("Size", &context->game.effects.trail_size_scale, 0.1f, 5.0f, "%.1f");
                        ImGui::SliderInt("Particles", &context->game.effects.trail_particle_count, 1, 20);

                        if (context->game.effects.current_trail_mode == EffectsManager::TrailMode::BLUR) ImGui::SliderInt("Blur Intensity", &context->game.effects.trail_blur_density, 2, 20);
                        if (context->game.effects.current_trail_mode == EffectsManager::TrailMode::LIGHT_STREAK) ImGui::SliderFloat("Streak Thickness", &context->game.effects.trail_streak_thickness, 1.0f, 10.0f, "%.1f");

                        ImGui::PopItemWidth(); ImGui::EndMenu();
                    }
                    ImGui::MenuItem("Ghost Moves", nullptr, &context->game.effects.enable_ghost_move);
                    ImGui::MenuItem("Smooth Hover Elevation", nullptr, &context->game.effects.enable_hover_elevation);
                    ImGui::MenuItem("Idle Rotation", nullptr, &context->game.effects.enable_rotation);
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Impact & Feedback")) {
                    if (ImGui::BeginMenu("Screen Shake (Trauma)")) {
                        ImGui::MenuItem("Enabled", nullptr, &context->game.effects.enable_shake);
                        ImGui::Separator();
                        ImGui::PushItemWidth(120);
                        ImGui::SliderFloat("Intensity", &context->game.effects.phys_shake_intensity, 0.0f, 2.0f, "%.1f");
                        ImGui::PopItemWidth(); ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Heavy Thud (Impact)")) {
                        ImGui::MenuItem("Enabled", nullptr, &context->game.effects.enable_thud);
                        ImGui::Separator();
                        ImGui::PushItemWidth(120);
                        ImGui::SliderFloat("Global Power", &context->game.effects.thud_global_intensity, 0.1f, 3.0f, "%.1f");
                        if (ImGui::CollapsingHeader("Advanced Tweaks")) {
                            ImGui::SliderFloat("Squash Factor", &context->game.effects.thud_squash_factor, 0.0f, 2.0f, "%.1f");
                            ImGui::SliderFloat("Flash Brightness", &context->game.effects.thud_flash_brightness, 0.0f, 2.0f, "%.1f");
                            ImGui::SliderFloat("Puff Density", &context->game.effects.thud_puff_density, 0.0f, 3.0f, "%.1f");
                            ImGui::SliderFloat("Ripple Scale", &context->game.effects.thud_ripple_scale, 0.0f, 3.0f, "%.1f");
                        }
                        ImGui::PopItemWidth(); ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Domino Impact Wave")) {
                        ImGui::MenuItem("Enabled", nullptr, &context->game.effects.enable_domino_wave);
                        ImGui::Separator();
                        ImGui::PushItemWidth(120);
                        ImGui::SliderFloat("Duration", &context->game.effects.domino_wave_duration, 0.1f, 5.0f, "%.2f s");
                        ImGui::SliderFloat("Intensity", &context->game.effects.domino_wave_intensity, 0.0f, 3.0f, "%.1f");
                        ImGui::SliderFloat("Range", &context->game.effects.domino_wave_range, 1.0f, 30.0f, "%.1f sq");
                        ImGui::SliderFloat("Speed", &context->game.effects.domino_wave_speed, 5.0f, 50.0f, "%.1f");
                        ImGui::SliderFloat("Bounce", &context->game.effects.domino_wave_bounce, 0.1f, 3.0f, "%.1f");
                        ImGui::SliderFloat("Glow", &context->game.effects.domino_wave_glow, 0.0f, 3.0f, "%.1f");
                        ImGui::SliderFloat("Wobble", &context->game.effects.domino_wave_wobble, 0.0f, 3.0f, "%.1f");
                        ImGui::PopItemWidth(); ImGui::EndMenu();
                    }
                    ImGui::MenuItem("Living Check Glow", nullptr, &context->game.effects.enable_living_check);
                    if (ImGui::BeginMenu("Combo Heat")) {
                        ImGui::MenuItem("Enabled", nullptr, &context->game.effects.enable_combo_heat);
                        ImGui::Separator();
                        ImGui::PushItemWidth(120);
                        const char* spark_styles[] = { "Classic", "Magical", "Radial Outward", "Vortex Inward" };
                        int current_spark = (int)context->game.effects.current_spark_style;
                        if (ImGui::BeginCombo("Spark Style", spark_styles[current_spark])) {
                            for (int n = 0; n < 4; n++) {
                                if (ImGui::Selectable(spark_styles[n], current_spark == n)) {
                                    context->game.effects.current_spark_style = (EffectsManager::SparkStyle)n;
                                }
                            }
                            ImGui::EndCombo();
                        }
                        ImGui::SliderFloat("Heat Intensity", &context->game.effects.heat_intensity, 0.1f, 3.0f, "%.1f");
                        ImGui::SliderFloat("Shake Intensity", &context->game.effects.combo_shake_intensity, 0.0f, 1.0f, "%.1f");
                        ImGui::SliderFloat("Text Timeout", &settings.state.combo_text_timeout, 1.0f, 60.0f, "%.0fs");
                        ImGui::PopItemWidth(); ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Celebrations")) {
                    if (ImGui::BeginMenu("Victory Dance")) {
                        ImGui::MenuItem("Enabled", nullptr, &context->game.effects.enable_victory_dance);
                        ImGui::Separator();
                        ImGui::PushItemWidth(120);
                        const char* dance_types[] = { "Classic Bounce", "V-Shape", "Swirl", "Wave" };
                        if (ImGui::BeginCombo("Type", dance_types[context->game.effects.victory_dance_type])) {
                            for (int n = 0; n < 4; n++) {
                                if (ImGui::Selectable(dance_types[n], context->game.effects.victory_dance_type == n)) {
                                    context->game.effects.victory_dance_type = n;
                                }
                            }
                            ImGui::EndCombo();
                        }
                        ImGui::SliderFloat("Intensity", &context->game.effects.victory_dance_intensity, 0.1f, 3.0f, "%.1f");
                        ImGui::SliderFloat("Duration", &context->game.effects.victory_dance_duration, 1.0f, 10.0f, "%.1f s");
                        ImGui::PopItemWidth(); ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Fireworks")) {
                        ImGui::MenuItem("Enabled", nullptr, &context->game.effects.enable_fireworks);
                        ImGui::Separator();
                        ImGui::PushItemWidth(120);
                        const char* fw_styles[] = { "Rings", "Red Shell", "Golden Willow" };
                        int current_fw = (int)context->game.effects.current_firework_style;
                        if (ImGui::BeginCombo("Style", fw_styles[current_fw])) {
                            for (int n = 0; n < 3; n++) {
                                if (ImGui::Selectable(fw_styles[n], current_fw == n)) {
                                    context->game.effects.current_firework_style = (EffectsManager::FireworkStyle)n;
                                }
                            }
                            ImGui::EndCombo();
                        }
                        ImGui::SliderInt("Quantity", &context->game.effects.fw_count, 1, 10);
                        if (ImGui::CollapsingHeader("Advanced Tweaks")) {
                            ImGui::SliderInt("Intensity", &context->game.effects.fw_particle_count, 10, 200);
                            ImGui::SliderFloat("Power", &context->game.effects.fw_explosion_power, 0.1f, 3.0f, "%.1f");
                            ImGui::SliderFloat("Speed", &context->game.effects.fw_particle_speed, 0.1f, 5.0f, "%.1f");
                            ImGui::SliderFloat("Duration", &context->game.effects.fw_duration_scale, 0.1f, 5.0f, "%.1f");
                            ImGui::SliderFloat("Size", &context->game.effects.fw_particle_size, 0.5f, 15.0f, "%.1f");
                            ImGui::SliderFloat("Gravity", &context->game.effects.fw_gravity_scale, 0.0f, 5.0f, "%.1f");
                        }
                        ImGui::PopItemWidth(); ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }            if (ImGui::BeginMenu("Settings")) {
                if (ImGui::BeginMenu("POV / Auto-Perspective")) {
                    static char pov_buf[128] = ""; strncpy_s(pov_buf, context->pov_player_name.c_str(), 127);
                    if (ImGui::InputText("Player Name", pov_buf, 128)) context->pov_player_name = pov_buf;
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("If a PGN game matches this name (White or Black),\nthe board will automatically flip to their perspective.");
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Appearance")) {
                    ImGui::PushItemWidth(150);
                    const char* app_themes[] = { "Deep Space", "Glassmorphism", "Cyberpunk", "Solar Light", "Custom Background", "Holographic Glass" };
                    if (ImGui::BeginCombo("App Theme", app_themes[settings.state.current_app_theme])) {
                        for (int n = 0; n < 6; n++) {
                            if (ImGui::Selectable(app_themes[n], settings.state.current_app_theme == n)) { settings.state.current_app_theme = n; ApplyTheme(); assets->PlaySound(assets->snd_select); }
                        }
                        ImGui::EndCombo();
                    }
                    if (settings.state.current_app_theme == 4) {
                        ImGui::SeparatorText("Background Image");
                        ImGui::Checkbox("Enable Image", &assets->theme_manager.bg_enabled);
                        if (ImGui::Button("Select Image...")) {
                            std::string path = OpenFileDialog("Images (*.jpg;*.png;*.bmp)\0*.jpg;*.png;*.bmp\0All Files (*.*)\0*.*\0", "Select Background");
                            if (!path.empty()) {
                                strncpy_s(assets->theme_manager.custom_bg_path, path.c_str(), 255);
                                if (assets->theme_manager.bg_texture) glDeleteTextures(1, &assets->theme_manager.bg_texture);
                                assets->theme_manager.bg_texture = AssetManager::LoadTextureFromFile(path.c_str());
                                assets->PlaySound(assets->snd_confirm);
                            }
                        }
                        if (assets->theme_manager.custom_bg_path[0] != '\0') ImGui::TextDisabled("File: %s", std::filesystem::path(assets->theme_manager.custom_bg_path).filename().string().c_str());
                        ImGui::SliderFloat("Brightness", &assets->theme_manager.bg_brightness, 0.0f, 1.0f);
                        ImGui::SliderFloat("Scale", &assets->theme_manager.bg_scale, 0.5f, 3.0f);
                    }
                    ImGui::Separator();
                    if (ImGui::BeginCombo("Piece Set", assets->theme_manager.current_piece_set.c_str())) {
                        for (int i = 0; i < (int)assets->theme_manager.available_piece_sets.size(); ++i) {
                            const auto& set = assets->theme_manager.available_piece_sets[i];
                            if (ImGui::Selectable(set.c_str(), assets->theme_manager.current_piece_set == set)) { 
                                assets->theme_manager.LoadPieceSet(set); 
                                assets->theme_manager.current_piece_theme_idx = i;
                                assets->PlaySound(assets->snd_select); 
                            }
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::SameLine(); if (ImGui::Button("Manage")) ImGui::OpenPopup("ManagePieceSets");
                    if (ImGui::BeginPopupModal("ManagePieceSets", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                        ImGui::Text("Installed Piece Sets:"); ImGui::Separator();
                        for (const auto& set : assets->theme_manager.available_piece_sets) {
                            ImGui::Text("%s", set.c_str()); ImGui::SameLine(200);
                            if (set == "gioco" || set == "Default") ImGui::TextDisabled("(System)");
                            else { ImGui::PushID(set.c_str()); if (ImGui::Button("Delete")) { assets->theme_manager.DeletePieceSet(set); assets->PlaySound(assets->snd_confirm); } ImGui::PopID(); }
                        }
                        ImGui::Separator(); if (ImGui::Button("Close", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
                        ImGui::EndPopup();
                    }
                    if (ImGui::BeginCombo("Board Theme", assets->theme_manager.current_board_theme.c_str())) {
                        for (int i = 0; i < (int)assets->theme_manager.available_board_themes.size(); ++i) {
                            const auto& theme = assets->theme_manager.available_board_themes[i];
                            if (ImGui::Selectable(theme.c_str(), assets->theme_manager.current_board_theme == theme)) { 
                                assets->theme_manager.LoadBoardTheme(theme); 
                                assets->theme_manager.current_board_theme_idx = i;
                                assets->PlaySound(assets->snd_select); 
                            }
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::SameLine(); if (ImGui::Button("Manage##board")) ImGui::OpenPopup("ManageBoardThemes");
                    if (ImGui::BeginPopupModal("ManageBoardThemes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                        ImGui::Text("Installed Board Themes:"); ImGui::Separator();
                        for (const auto& theme : assets->theme_manager.available_board_themes) {
                            ImGui::Text("%s", theme.c_str()); ImGui::SameLine(200);
                            if (theme == "Default") ImGui::TextDisabled("(System)");
                            else { ImGui::PushID(theme.c_str()); if (ImGui::Button("Delete")) { assets->theme_manager.DeleteBoardTheme(theme); assets->PlaySound(assets->snd_confirm); } ImGui::PopID(); }
                        }
                        ImGui::Separator(); if (ImGui::Button("Close", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
                        ImGui::EndPopup();
                    }
                    ImGui::Separator();
                    const char* chart_styles[] = { "Basic", "Dual", "Neon", "Hybrid" };
                    int current_chart = (int)context->game.chart_style;
                    if (ImGui::BeginCombo("Eval Style", chart_styles[current_chart])) {
                        for (int n = 0; n < 4; n++) { if (ImGui::Selectable(chart_styles[n], current_chart == n)) { context->game.chart_style = (GameState::EvalChartStyle)n; assets->PlaySound(assets->snd_select); } }
                        ImGui::EndCombo();
                    }
                    ImGui::PopItemWidth(); ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Interface & Font")) {
                    ImGui::PushItemWidth(120);
                    if (ImGui::BeginCombo("Font", assets->theme_manager.current_font_file.c_str())) {
                        for (const auto& font : assets->theme_manager.available_fonts) {
                            if (ImGui::Selectable(font.c_str(), assets->theme_manager.current_font_file == font)) { assets->theme_manager.current_font_file = font; ApplyTheme(); assets->PlaySound(assets->snd_select); }
                        }
                        ImGui::EndCombo();
                    }
                    if (ImGui::InputInt("Size", &assets->theme_manager.current_font_size)) { assets->theme_manager.current_font_size = std::clamp(assets->theme_manager.current_font_size, 8, 72); ApplyTheme(); }
                    ImGui::Separator();
                    ImGui::Text("Idling Settings");
                    ImGui::SliderFloat("Idle Delay", &context->game.effects.idle_delay_seconds, 1.0f, 60.0f, "%.0f s");
                    ImGui::SliderInt("Idle FPS", &context->game.effects.idle_fps_cap, 1, 30);
                    ImGui::Separator();
                    float current_vol = assets->master_volume;
                    if (ImGui::SliderFloat("Volume", &current_vol, 0.0f, 1.0f, "%.2f")) assets->SetVolume(current_vol);
                    ImGui::PopItemWidth(); ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Custom Assets")) {
                    ImGui::InputText("Piece Path", assets->theme_manager.custom_piece_path, 256);
                    if (ImGui::Button("Load Custom Pieces")) { assets->theme_manager.LoadPieceSet(assets->theme_manager.custom_piece_path, true); assets->PlaySound(assets->snd_confirm); }
                    ImGui::SameLine(); if (ImGui::Button("Import to Library")) { if (assets->theme_manager.ImportPieceSet(assets->theme_manager.custom_piece_path)) assets->PlaySound(assets->snd_confirm); else assets->PlaySound(assets->snd_wrong); }
                    ImGui::Separator();
                    ImGui::InputText("Board Path", assets->theme_manager.custom_board_path, 256);
                    if (ImGui::Button("Load Custom Board")) { assets->theme_manager.LoadBoardTheme(assets->theme_manager.custom_board_path, true); assets->PlaySound(assets->snd_confirm); }
                    ImGui::SameLine(); if (ImGui::Button("Import to Library##board")) { if (assets->theme_manager.ImportBoardTheme(assets->theme_manager.custom_board_path)) assets->PlaySound(assets->snd_confirm); else assets->PlaySound(assets->snd_wrong); }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Engines")) {
                if (ImGui::MenuItem("Stockfish Active", "S", &context->game.sf_active)) context->game.UpdateEngines(true);
                if (ImGui::MenuItem("Lc0 Active", "L", &context->game.lc0_active)) context->game.UpdateEngines(true);
                ImGui::Separator();
                if (ImGui::BeginMenu("Configure Stockfish")) {
                    bool changed = false;
                    if (ImGui::SliderInt("Threads", &context->game.sf_engine.threads, 1, std::thread::hardware_concurrency())) changed = true;
                    if (ImGui::InputInt("Hash (MB)", &context->game.sf_engine.hash)) changed = true;
                    if (ImGui::InputInt("MultiPV", &context->game.sf_engine.multipv)) changed = true;
                    if (ImGui::InputInt("Max Depth", &context->game.user_sf_depth)) { if (context->game.user_sf_depth < 1) context->game.user_sf_depth = 1; changed = true; }
                    if (ImGui::Checkbox("Ponder", &context->game.sf_engine.ponder)) changed = true;
                    if (ImGui::Checkbox("Show WDL", &context->game.sf_engine.show_wdl)) changed = true;
                    if (changed) { context->game.sf_engine.ApplyOptions(); context->game.UpdateEngines(true); }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Configure Lc0")) {
                    bool changed = false;
                    if (ImGui::InputInt("NNCacheSize", &context->game.lc0_engine.nn_cache_size)) changed = true;
                    if (ImGui::InputInt("MinibatchSize", &context->game.lc0_engine.minibatch_size)) changed = true;
                    if (ImGui::InputInt("MultiPV", &context->game.lc0_engine.multipv)) changed = true;
                    if (ImGui::InputInt("Max Nodes", &context->game.user_lc0_nodes)) { if (context->game.user_lc0_nodes < 1) context->game.user_lc0_nodes = 1; changed = true; }
                    if (ImGui::Checkbox("Ponder", &context->game.lc0_engine.ponder)) changed = true;
                    if (ImGui::Button("Load Weights...")) {
                        std::string path = OpenFileDialog("Weights (*.pb.gz;*.pb)\0*.pb.gz;*.pb\0All Files (*.*)\0*.*\0", "Select Lc0 Weights");
                        if (!path.empty()) { context->game.lc0_engine.weights_path = path; changed = true; }
                    }
                    if (changed) { context->game.lc0_engine.ApplyOptions(); context->game.UpdateEngines(true); }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("Shortcuts & Info", "H")) show_help = true;
                ImGui::EndMenu();
            }

            // Right-Aligned Engine Status
            struct StatusPart { std::string text; ImVec4 col; };
            std::vector<StatusPart> parts;

            if (context->game.auto_play_active) {
                std::string current_engine = "Human";
                std::string progress_text = "";
                chess::Color side = context->game.board.sideToMove();
                auto current_player = (side == chess::Color::WHITE) ? context->game.auto_play_white : context->game.auto_play_black;
                if (current_player == GameState::AutoPlayPlayer::STOCKFISH) {
                    current_engine = "SF";
                    int current_depth = context->game.sf_engine.GetInfo().depth;
                    int target_depth = (side == chess::Color::WHITE) ? context->game.auto_play_white_depth : context->game.auto_play_black_depth;
                    progress_text = "D:" + std::to_string(current_depth) + "/" + std::to_string(target_depth);
                } else if (current_player == GameState::AutoPlayPlayer::LC0) {
                    current_engine = "Lc0";
                    long long current_nodes = context->game.lc0_engine.GetInfo().nodes;
                    int target_nodes = (side == chess::Color::WHITE) ? context->game.auto_play_white_nodes : context->game.auto_play_black_nodes;
                    progress_text = "N:" + std::to_string(current_nodes) + "/" + std::to_string(target_nodes);
                }
                
                parts.push_back(StatusPart{ "[ Auto Play: ", ImVec4(0.4f, 0.8f, 1.0f, 1.0f) });
                parts.push_back(StatusPart{ (side == chess::Color::WHITE ? "W" : "B"), ImVec4(1.0f, 1.0f, 1.0f, 1.0f) });
                parts.push_back(StatusPart{ " (" + current_engine + ") ", ImVec4(0.8f, 0.8f, 0.8f, 1.0f) });
                if (current_player != GameState::AutoPlayPlayer::HUMAN) {
                    parts.push_back(StatusPart{ progress_text + " ] ", ImVec4(0.7f, 0.7f, 0.7f, 1.0f) });
                } else {
                    parts.push_back(StatusPart{ "Waiting ] ", ImVec4(0.7f, 0.7f, 0.7f, 1.0f) });
                }
            }

            auto AddEngineInfo = [&](EngineManager& eng, std::string name, bool is_lc0, bool active) {
                auto info = eng.GetInfo();
                bool searching = eng.IsSearching();
                ImVec4 dot_col = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
                if (active) dot_col = searching ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
                parts.push_back(StatusPart{ "[ ", ImVec4(0.7f, 0.7f, 0.7f, 1.0f) });
                parts.push_back(StatusPart{ "●", dot_col });
                parts.push_back(StatusPart{ " " + name + ": ", ImVec4(0.9f, 0.9f, 0.9f, 1.0f) });
                
                if (active && !info.lines.empty()) {
                    float cp = info.lines.begin()->second.score_cp / 100.0f;
                    if (context->game.board.sideToMove() == chess::Color::BLACK) cp = -cp;
                    ImVec4 eval_col = (cp >= 0) ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                    std::stringstream ess;
                    if (info.lines.begin()->second.score_mate != 0) { ess << "M" << info.lines.begin()->second.score_mate; eval_col = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); } 
                    else ess << (cp > 0 ? "+" : "") << std::fixed << std::setprecision(2) << cp;
                    parts.push_back({ ess.str(), eval_col });
                    
                    std::stringstream sss; sss << " | D" << info.depth << " | ";
                    if (is_lc0) sss << (int)info.nps << " n/s ] ";
                    else { if (info.nps < 1000000) sss << (int)(info.nps/1000) << " Kn/s ] "; else sss << std::fixed << std::setprecision(1) << (info.nps/1000000.0) << " Mn/s ] "; }
                    parts.push_back({ sss.str(), ImVec4(0.7f, 0.7f, 0.7f, 1.0f) });
                    if (is_lc0) { float gpu = 0.0f; if (!context->sys_monitor.gpuUsages.empty()) gpu = (float)context->sys_monitor.gpuUsages.begin()->second; parts.push_back({ "[ GPU: " + std::to_string((int)gpu) + "% ] ", ImVec4(0.8f, 0.6f, 1.0f, 1.0f) }); } 
                    else parts.push_back({ "[ CPU: " + std::to_string((int)context->sys_monitor.cpuUsage) + "% ] ", ImVec4(0.6f, 0.8f, 1.0f, 1.0f) });
                } else parts.push_back({ (active ? "Idle ] " : "Off ] "), ImVec4(0.5f, 0.5f, 0.5f, 1.0f) });
            };

            AddEngineInfo(context->game.sf_engine, "SF", false, context->game.sf_active);
            AddEngineInfo(context->game.lc0_engine, "Lc0", true, context->game.lc0_active);

            if (context->batch.active) parts.push_back({ "[ Batch: " + std::to_string(context->batch.current_game_idx + 1) + "/" + std::to_string(context->batch.report.total_games) + " ]", ImVec4(1.0f, 0.8f, 0.4f, 1.0f) });

            float total_w = 0.0f;
            for (const auto& part : parts) { if (part.text == "●") total_w += 12.0f; else total_w += ImGui::CalcTextSize(part.text.c_str()).x; }
            ImGui::SameLine(ImGui::GetWindowWidth() - total_w - 20);
            ImDrawList* menu_draw = ImGui::GetWindowDrawList();

            for (const auto& part : parts) {
                if (part.text == "●") {
                    ImVec2 cp = ImGui::GetCursorScreenPos(); float sz = ImGui::GetTextLineHeight();
                    menu_draw->AddCircleFilled(ImVec2(cp.x + 6, cp.y + sz * 0.5f), 4.0f, ImGui::ColorConvertFloat4ToU32(part.col)); ImGui::Dummy(ImVec2(12, sz));
                } else ImGui::TextColored(part.col, "%s", part.text.c_str());
                ImGui::SameLine(0, 0);
            }
            ImGui::EndMainMenuBar();
        }
    }

    void DrawHelpModal() {
        if (show_help) ImGui::OpenPopup("Shortcuts & Info");
        if (ImGui::BeginPopupModal("Shortcuts & Info", &show_help, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Controls:");
            ImGui::BulletText("Left/Right Arrows, Mouse Wheel: Navigate Move History");
            ImGui::BulletText("F: Flip Board");
            ImGui::BulletText("A: Toggle Auto-Next Puzzle");
            ImGui::BulletText("B: Toggle Blindfold Mode");
            ImGui::BulletText("R: Reset Current Puzzle");
            ImGui::BulletText("N: Next Random Puzzle");
            ImGui::BulletText("Z: Toggle Focus Mode");
            ImGui::BulletText("T: Toggle Timer");
            ImGui::BulletText("E: Toggle Engines Tab");
            ImGui::BulletText("S: Toggle Stockfish");
            ImGui::BulletText("L: Toggle Lc0");
            ImGui::BulletText("H: Toggle Help");
            ImGui::BulletText("Ctrl + Click (History): Open Puzzle on Lichess");
            ImGui::BulletText("Right Click (History): Delete Puzzle");
            
            ImGui::Separator();
            ImGui::Text("SRS Ratings (Color Legend):");
            auto ShowColor = [](ImVec4 col, const char* txt) { ImGui::ColorButton("##c", col, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(15, 15)); ImGui::SameLine(); ImGui::Text("%s", txt); };
            ShowColor(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "Failed (Needs Retry)");
            ShowColor(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "New / Unrated (Stage 0)");
            ShowColor(ImVec4(0.8f, 0.9f, 0.3f, 1.0f), "Learning (Stage 1)");
            ShowColor(ImVec4(0.6f, 0.9f, 0.4f, 1.0f), "Reviewing (Stage 2)");
            ShowColor(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "Proficient (Stage 3)");
            ShowColor(ImVec4(0.2f, 1.0f, 0.6f, 1.0f), "Mastered (Stage 4+)");

            ImGui::Separator();
            ImGui::Text("Visual Effects:");
            ImGui::BulletText("Piece Shake: Screen-shake on captures and errors.");
            ImGui::BulletText("Fireworks: Pyrotechnics when a puzzle is solved.");
            ImGui::BulletText("Piece Bounce: Subtle piece animation on selection/landing.");
            ImGui::BulletText("Trail: Fading motion-blur during moves.");
            ImGui::BulletText("Surge: Custom particle trails (Fire, Ice, etc.).");
            ImGui::BulletText("Screen Shake: Pieces shake nervously on wrong moves.");
            ImGui::BulletText("Rotation: Smooth 180-degree board rotation.");

            ImGui::Separator();
            if (ImGui::Button("Close", ImVec2(120, 0))) { show_help = false; ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
    }

    void RenderHistoryList() {
        context->puzzle_manager.RebuildHistoryCache();
        if (ImGui::Button("Start Due Reviews", ImVec2(-1, 0))) {
            int count = context->puzzle_manager.LoadDueReviews();
            context->puzzle_status = "Loaded " + std::to_string(count) + " reviews";
            if(count > 0) { context->game.review_mode = true; context->StartPuzzle(context->puzzle_manager.GetNextReviewPuzzle()); }
            assets->PlaySound(assets->snd_confirm);
        }
        if (!context->puzzle_manager.undo_stack.empty()) {
            if (ImGui::Button("Undo Delete")) { context->puzzle_manager.UndoDelete(); assets->PlaySound(assets->snd_confirm); }
            if (!context->puzzle_manager.redo_stack.empty()) { ImGui::SameLine(); if (ImGui::Button("Redo")) { context->puzzle_manager.RedoDelete(); assets->PlaySound(assets->snd_confirm); } }
        }
        ImGui::Separator();

        std::string today = context->puzzle_manager.GetCurrentDate();
        auto GetColor = [](int fails, int stage) -> ImVec4 {
            if (fails > 0) return ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
            if (stage == 0) return ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
            if (stage == 1) return ImVec4(0.8f, 0.9f, 0.3f, 1.0f);
            if (stage == 2) return ImVec4(0.6f, 0.9f, 0.4f, 1.0f);
            if (stage == 3) return ImVec4(0.4f, 0.9f, 0.5f, 1.0f);
            return ImVec4(0.2f, 1.0f, 0.6f, 1.0f);
        };

        ImGuiStyle& style = ImGui::GetStyle();
        float button_width = 20.0f; float spacing = style.ItemSpacing.x; float available_x = ImGui::GetContentRegionAvail().x;
        int items_per_row = std::max(1, (int)((available_x + spacing) / (button_width + spacing)));

        for (auto const& [date, indices] : context->puzzle_manager.grouped_user_puzzles) {
            std::string header_text = date + " (" + std::to_string(indices.size()) + ")";
            if (date == today) header_text = "Today (" + std::to_string(indices.size()) + ")";

            if (ImGui::CollapsingHeader(header_text.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                int row_count = (int)((indices.size() + items_per_row - 1) / items_per_row);
                ImGuiListClipper clipper; clipper.Begin(row_count);
                while (clipper.Step()) {
                    for (int r = clipper.DisplayStart; r < clipper.DisplayEnd; r++) {
                        for (int i = 0; i < items_per_row; i++) {
                            int n = r * items_per_row + i;
                            if (n >= (int)indices.size()) break;
                            int idx = indices[n]; const auto& p = context->puzzle_manager.user_puzzles[idx];
                            ImVec4 color = GetColor(p.fails_remaining, p.srs_stage);

                            ImGui::PushID(idx);
                            if (ImGui::ColorButton("##P", color, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(button_width, button_width))) {
                                if (ImGui::GetIO().KeyCtrl) {
                                    std::string url = "https://lichess.org/training/" + p.id;
                                    #ifdef _WIN32
                                    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
                                    #else
                                    SDL_OpenURL(url.c_str());
                                    #endif
                                } else {
                                    context->game.review_mode = false; context->puzzle_manager.ReplayPuzzle(idx); context->StartPuzzle(context->puzzle_manager.GetCurrentPuzzle()); assets->PlaySound(assets->snd_confirm);
                                }
                            }
                            if (ImGui::BeginPopupContextItem()) {
                                if (ImGui::MenuItem("Delete Puzzle")) { context->puzzle_manager.DeletePuzzle(p.id); assets->PlaySound(assets->snd_confirm); }
                                ImGui::EndPopup();
                            }
                            if (ImGui::IsItemHovered()) {
                                std::string tooltip = p.id + " | Rating: " + p.rating + " | Stage: " + std::to_string(p.srs_stage);
                                if (p.fails_remaining > 0) tooltip += " (Fails: " + std::to_string(p.fails_remaining) + ")";
                                tooltip += "\nLast: " + p.completion_date + " | Time: " + std::to_string((int)p.total_seconds_spent) + "s";
                                ImGui::SetTooltip("%s", tooltip.c_str());
                            }
                            ImGui::PopID();
                            if (i < items_per_row - 1 && n < (int)indices.size() - 1) ImGui::SameLine();
                        }
                    }
                }
                clipper.End();
            }
        }
    }

    void DrawStatsWindow() {
        if (!settings.state.show_stats) return;
        ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Statistics", &settings.state.show_stats)) {
            ImGui::Text("Monthly Activity");
            
            if (ImGui::Button("<")) { context->cal_state.month--; if (context->cal_state.month < 1) { context->cal_state.month = 12; context->cal_state.year--; } }
            ImGui::SameLine();
            if (ImGui::Button(">")) { context->cal_state.month++; if (context->cal_state.month > 12) { context->cal_state.month = 1; context->cal_state.year++; } }
            ImGui::SameLine();
            if (ImGui::Button("Today")) {
                std::time_t t = std::time(nullptr); std::tm now_tm;
                #if defined(_WIN32) || defined(_WIN64)
                    localtime_s(&now_tm, &t);
                #else
                    localtime_r(&t, &now_tm);
                #endif
                context->cal_state.year = now_tm.tm_year + 1900; context->cal_state.month = now_tm.tm_mon + 1;
            }
            ImGui::SameLine();
            ImGui::Text("%d-%02d", context->cal_state.year, context->cal_state.month);
            ImGui::Separator();
            
            if (ImGui::BeginTable("Calendar", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                const char* days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
                for (const char* d : days) ImGui::TableSetupColumn(d);
                ImGui::TableHeadersRow();
                
                int daysInMonth = GetDaysInMonth(context->cal_state.year, context->cal_state.month);
                int startDay = GetStartWeekday(context->cal_state.year, context->cal_state.month);
                int currentDay = 1;
                std::map<std::string, int> stats = context->puzzle_manager.GetStats();
                
                for (int week = 0; week < 6; ++week) {
                    ImGui::TableNextRow();
                    for (int day = 0; day < 7; ++day) {
                        ImGui::TableNextColumn();
                        if ((week == 0 && day < startDay) || currentDay > daysInMonth) continue;
                        std::stringstream ss; ss << context->cal_state.year << "-" << std::setw(2) << std::setfill('0') << context->cal_state.month << "-" << std::setw(2) << std::setfill('0') << currentDay;
                        std::string dateStr = ss.str();
                        int count = stats[dateStr];
                        
                        ImU32 cellColor = IM_COL32(50, 50, 50, 255); 
                        if (count > 0) { int intensity = std::min(255, 50 + count * 20); cellColor = IM_COL32(0, intensity, 0, 255); }
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, cellColor); ImGui::Text("%d", currentDay);
                        if (count > 0 && ImGui::IsItemHovered()) ImGui::SetTooltip("%s: %d solved", dateStr.c_str(), count);
                        currentDay++;
                    }
                    if (currentDay > daysInMonth) break;
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    void DrawEngineTab() {
        if (!settings.state.show_engine_tab) return;
        ImGui::SetNextWindowSize(ImVec2(400, 250), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Engines", &settings.state.show_engine_tab)) {
            if (!context->game.sf_active && !context->game.lc0_active) ImGui::Text("No engines active. Enable them in Settings or press S/L.");
            
            auto RenderEngineStats = [&](EngineManager& eng, bool& active, bool& show_eval_bar) {
                ImGui::PushID(eng.name.c_str());
                ImGui::AlignTextToFramePadding(); ImGui::SetWindowFontScale(1.2f); ImGui::Text("%s", eng.name.c_str()); ImGui::SetWindowFontScale(1.0f);
                ImGui::SameLine();
                if (active) { if (ImGui::Button("Stop")) { active = false; show_eval_bar = false; context->game.UpdateEngines(true); } }
                else { if (ImGui::Button("Start")) { active = true; show_eval_bar = true; context->game.UpdateEngines(true); } }
                ImGui::SameLine();
                ImGui::Checkbox("Show Eval Bar", &show_eval_bar);
                
                ImGui::SameLine();
                auto info = eng.GetInfo();
                if (!active) { 
                    if (info.depth > 0) ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Stopped at Depth %d", info.depth);
                    else ImGui::TextDisabled("Engine is stopped."); 
                    ImGui::Separator();
                    ImGui::PopID(); ImGui::Dummy(ImVec2(0, 10)); return; 
                }
                
                if (eng.IsSearching()) {
                    float pulse = (sinf((float)ImGui::GetTime() * 5.0f) * 0.5f + 0.5f);
                    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 0.5f + 0.5f * pulse), "Calculating...");
                } else if (!info.lines.empty()) {
                    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Analysis Complete (Depth %d)", info.depth);
                } else {
                    ImGui::TextDisabled("Waiting for engine...");
                }
                ImGui::Separator();

                if (!info.lines.empty()) {
                    if (ImGui::BeginTable("EngineStatsTable", 4, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                        ImGui::TableSetupColumn("Eval", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                        ImGui::TableSetupColumn("Line", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed, 65.0f);
                        ImGui::TableSetupColumn("Depth", ImGuiTableColumnFlags_WidthFixed, 40.0f);
                        
                        for (auto const& [id, line] : info.lines) {
                            ImGui::PushID(id);
                            ImGui::TableNextRow();
                            
                            // Eval
                            ImGui::TableNextColumn();
                            ImGui::AlignTextToFramePadding();
                            float cp_line = line.score_cp / 100.0f;
                            if (context->game.board.sideToMove() == chess::Color::BLACK) cp_line = -cp_line;
                            
                            if (line.score_mate != 0) {
                                ImGui::TextColored(line.score_mate > 0 ? ImVec4(0.4f, 1.0f, 0.4f, 1) : ImVec4(1.0f, 0.4f, 0.4f, 1), "M%d", std::abs(line.score_mate));
                            } else {
                                ImGui::TextColored(cp_line >= 0 ? ImVec4(0.8f, 1.0f, 0.8f, 1) : ImVec4(1.0f, 0.8f, 0.8f, 1), "%s%.2f", cp_line > 0 ? "+" : "", cp_line);
                            }

                            // Line (UCI to SAN)
                            ImGui::TableNextColumn();
                            ImGui::AlignTextToFramePadding();
                            std::string display_pv; std::vector<std::string> move_tokens;
                            if (!line.pv.empty()) {
                                std::stringstream pv_ss(line.pv); std::string move_str;
                                chess::Board temp_board = context->game.board;
                                while (pv_ss >> move_str) {
                                    move_tokens.push_back(move_str);
                                    chess::Movelist moves; chess::movegen::legalmoves<>(moves, temp_board);
                                    chess::Move m = chess::Move::NO_MOVE;
                                    for (const auto& lm : moves) { if (chess::uci::moveToUci(lm) == move_str) { m = lm; break; } }
                                    if (m != chess::Move::NO_MOVE) {
                                        std::string san = chess::uci::moveToSan(temp_board, m);
                                        
                                        if (!display_pv.empty()) display_pv += " ";
                                        if (temp_board.sideToMove() == chess::Color::WHITE) {
                                            display_pv += std::to_string(temp_board.fullMoveNumber()) + ". ";
                                        } else if (move_tokens.size() == 1) {
                                            display_pv += std::to_string(temp_board.fullMoveNumber()) + "... ";
                                        }
                                        display_pv += san;
                                        temp_board.makeMove(m);
                                    } else {
                                        if (!display_pv.empty()) display_pv += " ";
                                        display_pv += move_str; 
                                    }
                                }
                                
                                // Highlight top line
                                if (id == 1) {
                                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", display_pv.c_str());
                                } else {
                                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", display_pv.c_str());
                                }
                                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Depth: %d\nUCI: %s", info.depth, line.pv.c_str());
                            }
                            
                            // Preview Button
                            ImGui::TableNextColumn();
                            bool is_this_preview = (context->game.preview_mode && context->game.preview_moves.size() > 0 && move_tokens.size() > 0 && chess::uci::moveToUci(context->game.preview_moves[0]) == move_tokens[0]);
                            if (ImGui::Button(is_this_preview ? "Stop" : "Preview")) {
                                if (is_this_preview) { 
                                    context->game.preview_mode = false; 
                                    context->game.preview_moves.clear(); 
                                } else {
                                    context->game.preview_mode = true; context->game.preview_animating = true; context->game.preview_anim_idx = 0; context->game.preview_moves.clear();
                                    chess::Board temp_board = context->game.board; chess::Movelist moves; int limit = std::min((int)move_tokens.size(), 4);
                                    for (int i=0; i<limit; ++i) {
                                        chess::movegen::legalmoves<>(moves, temp_board); chess::Move m = chess::Move::NO_MOVE;
                                        for (const auto& lm : moves) { if (chess::uci::moveToUci(lm) == move_tokens[i]) { m = lm; break; } }
                                        if (m != chess::Move::NO_MOVE) { context->game.preview_moves.push_back(m); temp_board.makeMove(m); } else break;
                                    }
                                }
                            }
                            
                            // Depth
                            ImGui::TableNextColumn();
                            ImGui::AlignTextToFramePadding();
                            ImGui::TextDisabled("D%d", info.depth);
                            
                            ImGui::PopID();
                        }
                        ImGui::EndTable();
                        
                        if (info.nps > 0) {
                            ImGui::Dummy(ImVec2(0, 5));
                            if (info.nps < 1000000) ImGui::TextDisabled("Speed: %.1f kN/s", info.nps / 1000.0f);
                            else ImGui::TextDisabled("Speed: %.1f MN/s", info.nps / 1000000.0f);
                        }
                    }
                }
                ImGui::PopID(); ImGui::Dummy(ImVec2(0, 10)); 
            };
            RenderEngineStats(context->game.sf_engine, context->game.sf_active, settings.state.show_eval_bar_sf); RenderEngineStats(context->game.lc0_engine, context->game.lc0_active, settings.state.show_eval_bar_lc0);
        }
        ImGui::End();
    }

    void DrawComboCounter() {
        if (!context->game.effects.enable_combo_heat || context->game.combo_count < 3 || combo_inactivity_timer > settings.state.combo_text_timeout) return;
        
        ImDrawList* draw_list = ImGui::GetForegroundDrawList();
        char buf[32]; sprintf_s(buf, "%d COMBO", context->game.combo_count);
        
        // --- Kinetic Energy Math ---
        float time = (float)ImGui::GetTime();
        float pop_factor = 0.0f;
        if (combo_pop_timer > 0.0f) {
            float t = (0.5f - combo_pop_timer) / 0.5f;
            pop_factor = sinf(t * 3.14159f) * expf(-t * 3.0f); // Elastic bounce curve
        }

        float font_size = 40.0f + pop_factor * 30.0f;
        if (context->game.combo_count >= 20) font_size += 20.0f;
        if (context->game.combo_count >= 40) font_size += 20.0f;

        // Jitter at high combos
        ImVec2 jitter = {0,0};
        if (context->game.combo_count >= 15) {
            float jit_intensity = std::min(4.0f, (float)(context->game.combo_count - 15) * 0.2f);
            jitter.x = (float)((rand() % 100) - 50) / 100.0f * jit_intensity;
            jitter.y = (float)((rand() % 100) - 50) / 100.0f * jit_intensity;
        }

        int combo = context->game.combo_count; int r, g, b;
        
        auto GetComboColor = [](int c, int& r, int& g, int& b) {
            struct Stage { int c; int r, g, b; };
            const Stage stages[] = {
                { 3,  148, 0, 211 },  // Violet
                { 6,  75,  0, 130 },  // Indigo
                { 10, 0,   0, 255 },  // Blue
                { 14, 0,   255, 0 },  // Green
                { 18, 255, 255, 0 },  // Yellow
                { 22, 255, 127, 0 },  // Orange
                { 26, 255, 0,   0 },  // Red
                { 30, 100, 40,  20 }, // Deep Brown
                { 35, 15,  15,  20 }  // Cosmic Black
            };
            int num_stages = sizeof(stages) / sizeof(stages[0]);
            
            if (c <= stages[0].c) { r = stages[0].r; g = stages[0].g; b = stages[0].b; return; }
            if (c >= stages[num_stages - 1].c) { r = stages[num_stages - 1].r; g = stages[num_stages - 1].g; b = stages[num_stages - 1].b; return; }
            
            for (int i = 0; i < num_stages - 1; ++i) {
                if (c >= stages[i].c && c < stages[i+1].c) {
                    float t = (float)(c - stages[i].c) / (float)(stages[i+1].c - stages[i].c);
                    r = (int)(stages[i].r + t * (stages[i+1].r - stages[i].r));
                    g = (int)(stages[i].g + t * (stages[i+1].g - stages[i].g));
                    b = (int)(stages[i].b + t * (stages[i+1].b - stages[i].b));
                    return;
                }
            }
        };

        GetComboColor(combo, r, g, b);

        ImFont* combo_font = font_large ? font_large : ImGui::GetFont();
        ImVec2 text_size = combo_font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, buf);
        ImVec2 base_pos(last_board_pos.x + last_board_size * 0.5f - text_size.x * 0.5f, last_board_pos.y - text_size.y - 20.0f);
        ImVec2 pos(base_pos.x + jitter.x + context->game.effects.GetShakeOffset().x, base_pos.y + jitter.y + context->game.effects.GetShakeOffset().y);
        
        // Multi-layered shadow for glow effect
        for (int i = 4; i > 0; i--) {
            float alpha = (float)(100 / i);
            
            int shadow_r = 0, shadow_g = 0, shadow_b = 0;
            // Inverted Glow logic for Cosmic Black
            if (combo >= 32) {
                float blend = std::min(1.0f, (combo - 32) / 3.0f);
                shadow_r = (int)(blend * 255);
                shadow_g = (int)(blend * 250);
                shadow_b = (int)(blend * 200);
            }
            
            draw_list->AddText(combo_font, font_size, ImVec2(pos.x + i, pos.y + i), IM_COL32(shadow_r, shadow_g, shadow_b, (int)alpha), buf);
            if (combo >= 20) {
                draw_list->AddText(combo_font, font_size, ImVec2(pos.x - i, pos.y - i), IM_COL32(r, g, b, (int)(alpha * 0.3f)), buf);
            }
        }
        draw_list->AddText(combo_font, font_size, pos, IM_COL32(r, g, b, 255), buf);
    }

    void DrawHistoryDrilldownModal() {
        if (!show_drilldown_modal) return;

        ImGui::SetNextWindowSize(ImVec2(450, 500), ImGuiCond_FirstUseEver);
        if (ImGui::BeginPopupModal("Day Review", &show_drilldown_modal)) {
            ImGui::TextColored(ImVec4(0, 1, 1, 1), "Activity for %s", drilldown_date.c_str());
            ImGui::Separator();

            if (context->stats.state.daily_history.count(drilldown_date)) {
                const auto& day = context->stats.state.daily_history.at(drilldown_date);
                float acc = (day.puzzles_attempted > 0) ? (float)day.puzzles_solved / day.puzzles_attempted * 100.0f : 0.0f;
                
                ImGui::Columns(2, "DayStats", false);
                ImGui::BulletText("Solved: %d / %d", day.puzzles_solved, day.puzzles_attempted);
                ImGui::BulletText("Accuracy: %.1f%%", acc);
                ImGui::NextColumn();
                ImGui::BulletText("Time: %.1f min", day.total_time_spent / 60.0f);
                ImGui::BulletText("Avg TTI: %.1f s", (day.puzzles_attempted > 0) ? day.total_tti / day.puzzles_attempted : 0.0f);
                ImGui::Columns(1);
            } else {
                ImGui::TextDisabled("No detailed stats for this day.");
            }

            ImGui::Dummy(ImVec2(0, 10));
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Puzzles History:");
            ImGui::Separator();

            ImGui::BeginChild("DrilldownPuzzles", ImVec2(0, -40), true);
            bool found_any = false;
            for (int i = 0; i < (int)context->puzzle_manager.user_puzzles.size(); ++i) {
                const auto& p = context->puzzle_manager.user_puzzles[i];
                if (p.completion_date == drilldown_date) {
                    found_any = true;
                    ImGui::PushID(i);
                    ImGui::Text("[%s] Rating: %s", p.id.c_str(), p.rating.c_str());
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 70);
                    if (ImGui::Button("Replay")) {
                        context->game.review_mode = false;
                        context->puzzle_manager.ReplayPuzzle(i);
                        context->StartPuzzle(context->puzzle_manager.GetCurrentPuzzle());
                        assets->PlaySound(assets->snd_confirm);
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to replay this specific puzzle.");
                    
                    ImGui::TextDisabled("Themes: %s", p.themes.c_str());
                    ImGui::Separator();
                    ImGui::PopID();
                }
            }
            if (!found_any) ImGui::TextDisabled("No individual puzzle records found for this date.");
            ImGui::EndChild();

            if (ImGui::Button("Close", ImVec2(-1, 0))) {
                show_drilldown_modal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void DrawAICoach() {
        if (!settings.state.show_ai_coach) return;
        ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("AI Coach", &settings.state.show_ai_coach)) {
            
            // Configuration toggle
            if (ImGui::Button(context->llm_coach.show_config ? "Hide Config" : "Show Config")) {
                context->llm_coach.show_config = !context->llm_coach.show_config;
            }

            if (context->llm_coach.show_config) {
                ImGui::SeparatorText("Configuration");
                ImGui::InputText("API Endpoint", &context->llm_coach.endpoint_url[0], 256);
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    context->llm_coach.endpoint_url.assign(context->llm_coach.endpoint_url.c_str());
                }
                ImGui::InputText("Model Name", &context->llm_coach.model_name[0], 128);
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    context->llm_coach.model_name.assign(context->llm_coach.model_name.c_str());
                }
                ImGui::Dummy(ImVec2(0, 5));
            }

            ImGui::SeparatorText("Ask the Coach");
            ImGui::TextDisabled("Ask a specific question or leave blank for a general evaluation.");
            ImGui::InputTextMultiline("##user_input", context->llm_coach.user_input_buf, 1024, ImVec2(-1, 60));

            ImGui::Dummy(ImVec2(0, 10));

            if (context->llm_coach.is_thinking) {
                float pulse = (sinf((float)ImGui::GetTime() * 5.0f) * 0.5f + 0.5f);
                ImGui::TextColored(ImVec4(0, 1, 0, pulse), "AI is analyzing the position...");
            } else {
                if (ImGui::Button("Generate Explanation", ImVec2(-1, 40))) {
                    std::string fen = context->game.board.getFen();
                    std::string sf_eval = "unavailable";
                    std::string lc0_eval = "unavailable";

                    if (context->game.sf_active && !context->game.sf_engine.GetInfo().lines.empty()) {
                        float cp = context->game.sf_engine.GetInfo().lines.begin()->second.score_cp / 100.0f;
                        if (context->game.board.sideToMove() == chess::Color::BLACK) cp = -cp;
                        std::stringstream ss; ss << (cp > 0 ? "+" : "") << std::fixed << std::setprecision(2) << cp;
                        ss << " (Depth " << context->game.sf_engine.GetInfo().depth << ")";
                        sf_eval = ss.str();
                    }
                    if (context->game.lc0_active && !context->game.lc0_engine.GetInfo().lines.empty()) {
                        float cp = context->game.lc0_engine.GetInfo().lines.begin()->second.score_cp / 100.0f;
                        if (context->game.board.sideToMove() == chess::Color::BLACK) cp = -cp;
                        std::stringstream ss; ss << (cp > 0 ? "+" : "") << std::fixed << std::setprecision(2) << cp;
                        ss << " (Depth " << context->game.lc0_engine.GetInfo().depth << ")";
                        lc0_eval = ss.str();
                    }

                    std::string question(context->llm_coach.user_input_buf);
                    context->llm_coach.RequestAnalysis(fen, sf_eval, lc0_eval, question);
                }
            }

            ImGui::Dummy(ImVec2(0, 10));
            
            if (!context->llm_coach.response_text.empty()) {
                ImGui::BeginChild("CoachResponse", ImVec2(0, -30), true, ImGuiWindowFlags_HorizontalScrollbar);
                ImGui::TextWrapped("%s", context->llm_coach.response_text.c_str());
                ImGui::EndChild();
            }

            if (!context->llm_coach.metrics_text.empty()) {
                ImGui::TextDisabled("%s", context->llm_coach.metrics_text.c_str());
            }
        }
        ImGui::End();
    }

    void DrawPerformanceAnalyticsWindow() {
        if (!settings.state.show_performance_analytics) return;
        ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Tactical Insights", &settings.state.show_performance_analytics)) {
            if (ImGui::BeginTabBar("InsightsTabs")) {
                
                if (ImGui::BeginTabItem("TIQ Trend")) {
                    ImGui::Dummy(ImVec2(0, 5));
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), " TACTICAL MASTERY EVOLUTION");
                    ImGui::Separator();
                    
                    const auto& history = context->stats.state.tiq_history;
                    if (history.size() < 2) {
                        ImGui::Dummy(ImVec2(0, 50));
                        ImGui::TextDisabled("   Solve at least 5 puzzles to generate your evolutionary data.");
                    } else {
                        ImDrawList* dl = ImGui::GetWindowDrawList();
                        ImVec2 p = ImGui::GetCursorScreenPos();
                        ImVec2 avail = ImGui::GetContentRegionAvail();
                        float H = 280.0f;
                        ImVec2 sz(avail.x, H);

                        // 1. Background Frame & Rank Zones
                        dl->AddRectFilled(p, ImVec2(p.x + sz.x, p.y + sz.y), IM_COL32(10, 12, 16, 180), 6.0f); // Base Dark
                        
                        // Calculate Scale
                        float min_v = 10000.0f, max_v = -10000.0f;
                        for (float v : history) { if (v < min_v) min_v = v; if (v > max_v) max_v = v; }
                        float range = std::max(100.0f, max_v - min_v);
                        min_v -= range * 0.1f; max_v += range * 0.1f;
                        
                        auto ValToY = [&](float v) { return p.y + sz.y - ((v - min_v) / (max_v - min_v)) * sz.y; };

                        // Draw Zones (if visible in range)
                        auto DrawZone = [&](float y_start, float y_end, ImU32 col) {
                             float y1 = ValToY(y_start); float y2 = ValToY(y_end);
                             // Clamp to chart area
                             y1 = std::clamp(y1, p.y, p.y + sz.y);
                             y2 = std::clamp(y2, p.y, p.y + sz.y);
                             if (std::abs(y1 - y2) > 1.0f) dl->AddRectFilled(ImVec2(p.x, std::min(y1, y2)), ImVec2(p.x + sz.x, std::max(y1, y2)), col);
                        };
                        
                        // Apprentice (<1200) - Grey/Blue
                        DrawZone(min_v, 1200.0f, IM_COL32(30, 30, 40, 100));
                        // Candidate (1200-1600) - Bronze
                        DrawZone(1200.0f, 1600.0f, IM_COL32(100, 60, 20, 80));
                        // Expert (1600-2000) - Silver/Blue
                        DrawZone(1600.0f, 2000.0f, IM_COL32(40, 60, 100, 80));
                        // Master (>2000) - Gold
                        DrawZone(2000.0f, max_v, IM_COL32(100, 90, 20, 80));

                        dl->AddRect(p, ImVec2(p.x + sz.x, p.y + sz.y), IM_COL32(255, 255, 255, 15), 6.0f); // Border

                        // 3. Grid & Labels
                        int steps = 5;
                        for (int i = 0; i <= steps; i++) {
                            float t = (float)i / steps;
                            float val = min_v + t * (max_v - min_v);
                            float y = ValToY(val);
                            dl->AddLine(ImVec2(p.x, y), ImVec2(p.x + sz.x, y), IM_COL32(255, 255, 255, 10));
                            char buf[16]; snprintf(buf, 16, "%.0f", val);
                            dl->AddText(ImVec2(p.x + 5, y - 15), IM_COL32(255, 255, 255, 80), buf);
                        }

                        // 4. Raw Data Curve (Faint Green)
                        std::vector<ImVec2> points;
                        float dx = sz.x / (history.size() - 1);
                        for (size_t i = 0; i < history.size(); ++i) {
                            points.push_back(ImVec2(p.x + i * dx, ValToY(history[i])));
                        }

                        if (points.size() > 1) {
                            // Fill Path (Manual Tessellation for robustness against non-convex shapes)
                            float base_y = p.y + sz.y;
                            ImU32 fill_col = IM_COL32(0, 255, 128, 20);
                            
                            for (size_t i = 1; i < points.size(); ++i) {
                                ImVec2 p0 = points[i-1]; ImVec2 p1 = points[i];
                                float cp_dx = (p1.x - p0.x) * 0.5f;
                                ImVec2 cp0 = ImVec2(p0.x + cp_dx, p0.y);
                                ImVec2 cp1 = ImVec2(p1.x - cp_dx, p1.y);
                                
                                int segments = 10;
                                for (int j = 0; j < segments; ++j) {
                                    float t0 = (float)j / segments;
                                    float t1 = (float)(j + 1) / segments;
                                    
                                    // Cubic Bezier Eval
                                    auto Bezier = [&](float t) {
                                        float u = 1.0f - t;
                                        float tt = t * t; float uu = u * u;
                                        float uuu = uu * u; float ttt = tt * t;
                                        return ImVec2(
                                            uuu * p0.x + 3 * uu * t * cp0.x + 3 * u * tt * cp1.x + ttt * p1.x,
                                            uuu * p0.y + 3 * uu * t * cp0.y + 3 * u * tt * cp1.y + ttt * p1.y
                                        );
                                    };
                                    
                                    ImVec2 b0 = Bezier(t0);
                                    ImVec2 b1 = Bezier(t1);
                                    
                                    dl->AddQuadFilled(
                                        b0, b1, 
                                        ImVec2(b1.x, base_y), ImVec2(b0.x, base_y), 
                                        fill_col
                                    );
                                }
                            }

                            // Stroke Path (Smooth Outline)
                            dl->PathClear();
                            dl->PathLineTo(points[0]); 
                            for (size_t i = 1; i < points.size(); ++i) {
                                ImVec2 p0 = points[i-1]; ImVec2 p1 = points[i];
                                float cp_dx = (p1.x - p0.x) * 0.5f;
                                dl->PathBezierCubicCurveTo(ImVec2(p0.x + cp_dx, p0.y), ImVec2(p1.x - cp_dx, p1.y), p1, 0);
                            }
                            dl->PathStroke(IM_COL32(0, 255, 128, 100), 0, 1.5f); 
                        }

                        // 5. SMA-10 Trend Line (Gold)
                        if (history.size() >= 5) {
                            std::vector<ImVec2> sma_points;
                            int period = std::min((int)history.size(), 10);
                            for (size_t i = 0; i < history.size(); ++i) {
                                if (i < (size_t)(period - 1)) {
                                    // Not enough data for full SMA, use partial or just skip drawing line
                                    // For visual continuity, we can just use the raw value or average of what we have
                                    float sum = 0; for(size_t k=0; k<=i; ++k) sum += history[k];
                                    sma_points.push_back(ImVec2(p.x + i * dx, ValToY(sum / (i + 1))));
                                } else {
                                    float sum = 0;
                                    for (int k = 0; k < period; ++k) sum += history[i - k];
                                    sma_points.push_back(ImVec2(p.x + i * dx, ValToY(sum / period)));
                                }
                            }
                            
                            dl->PathClear();
                            for (size_t i = 0; i < sma_points.size(); ++i) {
                                if (i == 0) dl->PathLineTo(sma_points[0]);
                                else {
                                    ImVec2 p0 = sma_points[i-1]; ImVec2 p1 = sma_points[i];
                                    float cp_dx = (p1.x - p0.x) * 0.5f;
                                    dl->PathBezierCubicCurveTo(ImVec2(p0.x + cp_dx, p0.y), ImVec2(p1.x - cp_dx, p1.y), p1, 0);
                                }
                            }
                            dl->PathStroke(IM_COL32(255, 215, 0, 255), 0, 3.0f); // Thick Gold Line
                        }

                        // 6. Peak Indicator
                        float peak_val = -10000.0f; int peak_idx = -1;
                        for(size_t i=0; i<history.size(); ++i) { if(history[i] > peak_val) { peak_val = history[i]; peak_idx = (int)i; } }
                        
                        if (peak_idx != -1) {
                            ImVec2 peak_pos = points[peak_idx];
                            dl->AddCircleFilled(peak_pos, 4.0f, IM_COL32(255, 215, 0, 255));
                            dl->AddText(ImVec2(peak_pos.x - 10, peak_pos.y - 20), IM_COL32(255, 215, 0, 255), "PEAK");
                        }

                        // 7. Interactive Tooltip
                        ImVec2 mouse = ImGui::GetMousePos();
                        if (ImGui::IsWindowHovered() && mouse.x >= p.x && mouse.x <= p.x + sz.x && mouse.y >= p.y && mouse.y <= p.y + sz.y) {
                            float mouse_t = (mouse.x - p.x) / sz.x;
                            int idx = (int)(mouse_t * (history.size() - 1) + 0.5f);
                            idx = std::clamp(idx, 0, (int)history.size() - 1);
                            
                            ImVec2 hover_p = points[idx];
                            dl->AddCircleFilled(hover_p, 5.0f, IM_COL32(255, 255, 255, 255));
                            dl->AddCircle(hover_p, 8.0f, IM_COL32(0, 255, 128, 150), 0, 2.0f);
                            
                            ImGui::BeginTooltip();
                            ImGui::Text("TIQ: %.0f", history[idx]);
                            if (idx > 0) {
                                float diff = history[idx] - history[idx-1];
                                ImVec4 diff_col = (diff >= 0) ? ImVec4(0,1,0,1) : ImVec4(1,0.3f,0.3f,1);
                                ImGui::TextColored(diff_col, "%s%.0f", (diff >= 0 ? "+" : ""), diff);
                            }
                            ImGui::TextDisabled("Entry %d", idx + 1);
                            ImGui::EndTooltip();
                        }

                        ImGui::Dummy(ImVec2(0, H + 10));
                        
                        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + sz.y + 10.0f));
                        ImGui::Text("Current TIQ:"); ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0, 1, 1, 1), "%.0f", context->stats.state.current_tiq);
                    }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Theme Proficiency")) {
                    // 1. Data Preparation (Dimensions for Chart)
                    std::map<std::string, float> dim_ratings = context->stats.GetDimensionRatings();
                    std::vector<std::string> dim_order = { "Tactics", "Checkmate", "Advanced", "Endgame", "Opening/Def" };

                    // 2. Data Preparation (Individual Themes for List)
                    std::vector<std::pair<std::string, float>> top_themes;
                    for (auto const& [name, prof] : context->stats.state.theme_proficiencies) {
                        if (prof.attempts < 3) continue;
                        top_themes.push_back({name, prof.rating});
                    }
                    std::sort(top_themes.begin(), top_themes.end(), [](const auto& a, const auto& b) {
                        return a.second > b.second;
                    });
                    if (top_themes.size() > 5) top_themes.resize(5);

                    // Header
                    float avail_w = ImGui::GetContentRegionAvail().x;
                    const char* title = "SKILL DIMENSIONS";
                    ImVec2 title_sz = ImGui::CalcTextSize(title);
                    ImGui::SetCursorPosX((avail_w - title_sz.x) * 0.5f);
                    ImGui::TextColored(ImVec4(0, 1, 1, 1), "%s", title);
                    ImGui::Separator();

                    // 3. Radar Chart Drawing
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    float size = 280.0f;
                    
                    // Center the chart horizontally
                    float chart_offset_x = (avail_w - size) * 0.5f;
                    ImVec2 center(p.x + chart_offset_x + size * 0.5f, p.y + size * 0.5f + 10.0f);
                    float radius = size * 0.40f; 

                    // Background Webs
                    for (int r = 1; r <= 4; r++) {
                        float curr_r = radius * (r / 4.0f);
                        for (int i = 0; i < 5; i++) {
                            float ang1 = -1.57079f + (i * 6.28318f / 5.0f);
                            float ang2 = -1.57079f + ((i + 1) * 6.28318f / 5.0f);
                            dl->AddLine(
                                ImVec2(center.x + cosf(ang1) * curr_r, center.y + sinf(ang1) * curr_r),
                                ImVec2(center.x + cosf(ang2) * curr_r, center.y + sinf(ang2) * curr_r),
                                IM_COL32(255, 255, 255, 30));
                        }
                    }
                    // Axes & Labels
                    for (int i = 0; i < 5; i++) {
                        float ang = -1.57079f + (i * 6.28318f / 5.0f);
                        ImVec2 tip(center.x + cosf(ang) * radius, center.y + sinf(ang) * radius);
                        dl->AddLine(center, tip, IM_COL32(255, 255, 255, 50));
                        
                        // Labels
                        std::string lbl = dim_order[i];
                        ImVec2 txt_sz = ImGui::CalcTextSize(lbl.c_str());
                        float txt_dist = radius + 15.0f;
                        dl->AddText(ImVec2(center.x + cosf(ang) * txt_dist - txt_sz.x * 0.5f, center.y + sinf(ang) * txt_dist - txt_sz.y * 0.5f), IM_COL32(200, 200, 200, 200), lbl.c_str());
                    }

                    // Data Polygon
                    auto NormalizeRating = [](float r) { return std::clamp((r - 800.0f) / 1600.0f, 0.1f, 1.0f); }; // 800..2400 range

                    dl->PathClear();
                    for (int i = 0; i < 5; i++) {
                        float val = NormalizeRating(dim_ratings[dim_order[i]]);
                        float ang = -1.57079f + (i * 6.28318f / 5.0f);
                        float r = radius * val;
                        dl->PathLineTo(ImVec2(center.x + cosf(ang) * r, center.y + sinf(ang) * r));
                    }
                    dl->PathFillConvex(IM_COL32(0, 200, 255, 100));
                    
                    dl->PathClear();
                    for (int i = 0; i < 5; i++) {
                        float val = NormalizeRating(dim_ratings[dim_order[i]]);
                        float ang = -1.57079f + (i * 6.28318f / 5.0f);
                        float r = radius * val;
                        dl->PathLineTo(ImVec2(center.x + cosf(ang) * r, center.y + sinf(ang) * r));
                    }
                    dl->PathStroke(IM_COL32(0, 200, 255, 255), true, 2.0f);
                    
                    ImGui::Dummy(ImVec2(0, size + 25.0f)); // Spacer for canvas

                    // 4. Skill Bars (Centered below chart)
                    if (!top_themes.empty()) {
                        ImGui::Separator();
                        ImGui::Text("Top Rated Themes");
                        ImGui::Dummy(ImVec2(0, 2));
                        
                        for (const auto& t : top_themes) {
                            float rating = t.second;
                            ImGui::Text("%s", t.first.c_str());
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40);
                            ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "%.0f", rating);

                            float progress = std::clamp((rating - 800.0f) / 1600.0f, 0.0f, 1.0f);
                            
                            ImVec4 col = (rating > 2000) ? ImVec4(0.2f, 1.0f, 0.4f, 1) : ((rating > 1500) ? ImVec4(1.0f, 0.8f, 0.2f, 1) : ImVec4(1.0f, 0.3f, 0.3f, 1));
                            
                            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, col);
                            ImGui::ProgressBar(progress, ImVec2(-1, 6), "");
                            ImGui::PopStyleColor();
                            ImGui::Dummy(ImVec2(0, 2)); 
                        }
                    } else {
                        ImGui::TextDisabled("Solve puzzles to see your top themes here.");
                    }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Daily History")) {
                    ImGui::Dummy(ImVec2(0, 5));
                    
                    // Calculate Total Solved in Year
                    int total_solved_year = 0;
                    std::string year_prefix = std::to_string(context->cal_state.year) + "-";
                    for (auto const& [date, perf] : context->stats.state.daily_history) {
                        if (date.find(year_prefix) == 0) total_solved_year += perf.puzzles_solved;
                    }

                    // Header with Year Navigation, Solve Count & Legend
                    float avail_w = ImGui::GetContentRegionAvail().x;
                    
                    // Left Side: Solve Count
                    ImGui::TextDisabled("%d Puzzles Solved", total_solved_year);
                    ImGui::SameLine();

                    // Center: Year Title
                    std::string year_str = std::to_string(context->cal_state.year) + " ACTIVITY";
                    ImVec2 year_sz = ImGui::CalcTextSize(year_str.c_str());
                    float center_pos = (avail_w - year_sz.x) * 0.5f;
                    if (ImGui::GetCursorPosX() < center_pos) ImGui::SetCursorPosX(center_pos);
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", year_str.c_str());
                    ImGui::SameLine();

                    // Right Side: Legend
                    float legend_box_sz = 12.0f;
                    float legend_spacing = 2.0f;
                    float legend_w = ImGui::CalcTextSize("Less").x + (5 * (legend_box_sz + legend_spacing)) + ImGui::CalcTextSize("More").x + 15.0f;
                    ImGui::SetCursorPosX(avail_w - legend_w);
                    
                    ImGui::TextDisabled("Less"); ImGui::SameLine(0, 5);
                    ImDrawList* header_dl = ImGui::GetWindowDrawList();
                    ImVec2 legend_p = ImGui::GetCursorScreenPos();
                    ImU32 legend_cols[] = { IM_COL32(30, 30, 35, 255), IM_COL32(14, 68, 42, 255), IM_COL32(0, 109, 50, 255), IM_COL32(38, 166, 65, 255), IM_COL32(57, 211, 83, 255) };
                    for (int i = 0; i < 5; ++i) {
                        header_dl->AddRectFilled(legend_p, ImVec2(legend_p.x + legend_box_sz, legend_p.y + legend_box_sz), legend_cols[i], 2.0f);
                        legend_p.x += legend_box_sz + legend_spacing;
                    }
                    ImGui::Dummy(ImVec2(5 * (legend_box_sz + legend_spacing), legend_box_sz)); ImGui::SameLine(0, 5);
                    ImGui::TextDisabled("More");
                    
                    ImGui::Separator();
                    ImGui::Dummy(ImVec2(0, 10));

                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    
                    // Responsive Calculation (Adaptive Split Layout - 1, 2, or 3 rows)
                    int weeks_total = 54;
                    float spacing = 3.0f;
                    float min_sz = 18.0f; 
                    
                    int rows = 1;
                    if (avail_w < weeks_total * (min_sz + spacing)) rows = 2;
                    if (avail_w < (weeks_total / 2) * (min_sz + spacing)) rows = 3;
                    
                    int cols_per_row = (weeks_total + rows - 1) / rows;
                    
                    float sz = (avail_w - (cols_per_row - 1) * spacing) / cols_per_row;
                    sz = std::clamp(sz, 18.0f, 24.0f); 
                    
                    // Calculate Start Day of Year
                    std::tm time_in = {0};
                    time_in.tm_year = context->cal_state.year - 1900;
                    time_in.tm_mon = 0; // Jan
                    time_in.tm_mday = 1;
                    std::mktime(&time_in);
                    int start_wday = time_in.tm_wday; 

                    // Render Grid (Jan 1 to Dec 31)
                    int current_day_of_year = 0;
                    int days_in_year = (context->cal_state.year % 4 == 0 && (context->cal_state.year % 100 != 0 || context->cal_state.year % 400 == 0)) ? 366 : 365;
                    
                    float block_gap = 10.0f; // Reduced gap
                    
                    for (int w = 0; w < weeks_total; ++w) {
                        int grid_row = w / cols_per_row;
                        int grid_col = w % cols_per_row;
                        
                        float block_offset_y = grid_row * (7 * (sz + spacing) + block_gap);

                        for (int d = 0; d < 7; ++d) {
                            if (w == 0 && d < start_wday) continue;
                            if (current_day_of_year >= days_in_year) break;

                            std::tm cell_tm = time_in;
                            cell_tm.tm_mday += current_day_of_year;
                            std::mktime(&cell_tm);
                            
                            char buf[11]; std::strftime(buf, sizeof(buf), "%Y-%m-%d", &cell_tm);
                            std::string date_str(buf);
                            
                            int count = 0;
                            if (context->stats.state.daily_history.count(date_str)) {
                                count = context->stats.state.daily_history[date_str].puzzles_solved;
                            }

                            ImVec2 cell_p(p.x + grid_col * (sz + spacing), p.y + d * (sz + spacing) + block_offset_y);
                            ImU32 col = IM_COL32(30, 30, 35, 255); 
                            if (count > 0) {
                                if (count < 3) col = IM_COL32(14, 68, 42, 255);
                                else if (count < 6) col = IM_COL32(0, 109, 50, 255);
                                else if (count < 10) col = IM_COL32(38, 166, 65, 255);
                                else col = IM_COL32(57, 211, 83, 255);
                            }

                            dl->AddRectFilled(cell_p, ImVec2(cell_p.x + sz, cell_p.y + sz), col, 3.0f);

                            if (ImGui::IsMouseHoveringRect(cell_p, ImVec2(cell_p.x + sz, cell_p.y + sz))) {
                                dl->AddRect(cell_p, ImVec2(cell_p.x + sz, cell_p.y + sz), IM_COL32(255, 255, 255, 200), 3.0f);
                                ImGui::BeginTooltip();
                                char nice_date[32]; std::strftime(nice_date, sizeof(nice_date), "%a, %b %d", &cell_tm);
                                ImGui::Text("%s", nice_date);
                                ImGui::TextColored(ImVec4(0,1,0,1), "%d Solved", count);
                                ImGui::EndTooltip();

                                if (ImGui::IsMouseClicked(0)) {
                                    drilldown_date = date_str;
                                    show_drilldown_modal = true;
                                    ImGui::OpenPopup("Day Review");
                                    assets->PlaySound(assets->snd_select);
                                }
                            }
                            
                            current_day_of_year++;
                        }
                    }

                    float single_block_h = 7 * (sz + spacing);
                    float grid_height = (rows * single_block_h) + ((rows - 1) * block_gap);
                    float grid_width = cols_per_row * (sz + spacing);
                    
                    // Hover Navigation (Arrows)
                    ImVec2 mouse = ImGui::GetMousePos();
                    if (mouse.x >= p.x && mouse.x <= p.x + grid_width && mouse.y >= p.y && mouse.y <= p.y + grid_height) {
                        if (mouse.x < p.x + 40.0f) {
                            ImVec2 arrow_c(p.x + 15, p.y + grid_height * 0.5f);
                            dl->AddTriangleFilled(ImVec2(arrow_c.x + 5, arrow_c.y - 8), ImVec2(arrow_c.x + 5, arrow_c.y + 8), ImVec2(arrow_c.x - 5, arrow_c.y), IM_COL32(255, 255, 255, 150));
                            if (ImGui::IsMouseClicked(0)) { context->cal_state.year--; assets->PlaySound(assets->snd_select); }
                        }
                        else if (mouse.x > p.x + grid_width - 40.0f) {
                            ImVec2 arrow_c(p.x + grid_width - 15, p.y + grid_height * 0.5f);
                            dl->AddTriangleFilled(ImVec2(arrow_c.x - 5, arrow_c.y - 8), ImVec2(arrow_c.x - 5, arrow_c.y + 8), ImVec2(arrow_c.x + 5, arrow_c.y), IM_COL32(255, 255, 255, 150));
                            if (ImGui::IsMouseClicked(0)) { context->cal_state.year++; assets->PlaySound(assets->snd_select); }
                        }
                    }

                    ImGui::Dummy(ImVec2(0, grid_height + 30.0f)); 

                    // Annual Highlights Section
                    ImGui::SeparatorText("ANNUAL HIGHLIGHTS");
                    StatsManager::AnnualHighlights highlights = context->stats.GetAnnualHighlights(context->cal_state.year);

                    if (highlights.total_attempted == 0) {
                        ImGui::TextDisabled("No activity recorded for %d yet.", context->cal_state.year);
                    } else {
                        if (ImGui::BeginTable("HighlightsTable", 3, ImGuiTableFlags_NoBordersInBody)) {
                            ImGui::TableNextRow();
                            
                            // Column 1: Streak
                            ImGui::TableNextColumn();
                            ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "Longest Streak");
                            ImGui::Text("%d Days", highlights.longest_streak);
                            
                            // Column 2: Power Day
                            ImGui::TableNextColumn();
                            ImGui::TextColored(ImVec4(0, 1, 1, 1), "Power Day");
                            ImGui::Text("%d Solved", highlights.max_solved_day);
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Achieved on: %s", highlights.max_day_date.c_str());

                            // Column 3: Stats
                            ImGui::TableNextColumn();
                            ImGui::TextColored(ImVec4(0.4f, 1, 0.4f, 1), "Annual Accuracy");
                            ImGui::Text("%.1f%%", highlights.avg_accuracy);
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Solved: %d / Attempted: %d", highlights.total_solved, highlights.total_attempted);

                            ImGui::EndTable();
                        }
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Summary")) {
                    ImGui::Dummy(ImVec2(0, 5));
                    
                    StatsManager::RPGStats stats = context->stats.GetRPGStats();
                    
                    // Card Container
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    float w = ImGui::GetContentRegionAvail().x;
                    float h = 180.0f;
                    
                    // Card Background
                    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(20, 20, 25, 200), 8.0f);
                    dl->AddRect(p, ImVec2(p.x + w, p.y + h), IM_COL32(255, 215, 0, 50), 8.0f);
                    
                    // Avatar Area (Left)
                    float av_size = 100.0f;
                    ImVec2 av_center(p.x + 80.0f, p.y + h * 0.5f);
                    dl->AddCircleFilled(av_center, av_size * 0.5f, IM_COL32(50, 50, 60, 255));
                    dl->AddCircle(av_center, av_size * 0.5f, IM_COL32(255, 215, 0, 200), 0, 3.0f);
                    
                    // Initials
                    const char* initials = "GM"; // Placeholder
                    ImVec2 txt_sz = ImGui::CalcTextSize(initials);
                    ImGui::SetWindowFontScale(2.0f);
                    dl->AddText(ImVec2(av_center.x - txt_sz.x, av_center.y - txt_sz.y), IM_COL32(255, 255, 255, 255), initials);
                    ImGui::SetWindowFontScale(1.0f);
                    
                    // Title / Rank
                    std::string rank_title = "Apprentice";
                    if (stats.power > 80) rank_title = "Grandmaster";
                    else if (stats.power > 60) rank_title = "Master";
                    else if (stats.power > 40) rank_title = "Expert";
                    else if (stats.power > 20) rank_title = "Candidate";
                    
                    ImVec2 title_sz = ImGui::CalcTextSize(rank_title.c_str());
                    dl->AddText(ImVec2(av_center.x - title_sz.x * 0.5f, p.y + h - 30.0f), IM_COL32(200, 200, 200, 255), rank_title.c_str());

                    // Stats Area (Right)
                    float bar_x = p.x + 160.0f;
                    float bar_w = w - 180.0f;
                    float bar_h = 12.0f;
                    float bar_y = p.y + 25.0f;
                    float gap = 35.0f;

                    auto DrawStatBar = [&](const char* label, int val, ImU32 col, const char* icon) {
                        dl->AddText(ImVec2(bar_x, bar_y - 18), IM_COL32(200, 200, 200, 255), label);
                        char val_buf[16]; snprintf(val_buf, 16, "%d", val);
                        ImVec2 val_sz = ImGui::CalcTextSize(val_buf);
                        dl->AddText(ImVec2(bar_x + bar_w - val_sz.x, bar_y - 18), col, val_buf);
                        
                        // Bar BG
                        dl->AddRectFilled(ImVec2(bar_x, bar_y), ImVec2(bar_x + bar_w, bar_y + bar_h), IM_COL32(0, 0, 0, 100), 6.0f);
                        // Bar FG
                        float fill = (float)val / 100.0f;
                        if (fill > 0) dl->AddRectFilled(ImVec2(bar_x, bar_y), ImVec2(bar_x + bar_w * fill, bar_y + bar_h), col, 6.0f);
                        
                        bar_y += gap;
                    };

                    DrawStatBar("AGILITY (Speed)", stats.agility, IM_COL32(0, 255, 255, 255), "A");
                    DrawStatBar("WISDOM (Accuracy)", stats.wisdom, IM_COL32(180, 100, 255, 255), "W");
                    DrawStatBar("TENACITY (Streak)", stats.tenacity, IM_COL32(100, 255, 100, 255), "T");
                    DrawStatBar("POWER (Rating)", stats.power, IM_COL32(255, 100, 100, 255), "P");

                    ImGui::Dummy(ImVec2(0, h + 10.0f));

                    // Bio / Status
                    ImGui::TextColored(ImVec4(1, 1, 0, 1), "STATUS REPORT");
                    ImGui::Separator();
                    
                    // Today's Sharpness (DPI)
                    float dpi = context->stats.GetDailyDPI(context->stats.GetCurrentDate());
                    const char* sharpness_label = "SLUGGISH";
                    ImVec4 sharpness_col = ImVec4(1, 0.4f, 0.4f, 1);
                    if (dpi > 85) { sharpness_label = "CRUSHING IT"; sharpness_col = ImVec4(0, 1, 1, 1); }
                    else if (dpi > 70) { sharpness_label = "SHARP"; sharpness_col = ImVec4(0.4f, 1, 0.4f, 1); }
                    else if (dpi > 40) { sharpness_label = "STEADY"; sharpness_col = ImVec4(1, 1, 0.4f, 1); }

                    ImGui::Text("Tactical Sharpness:"); ImGui::SameLine();
                    ImGui::TextColored(sharpness_col, "%.1f [%s]", dpi, sharpness_label);
                    
                    ImGui::TextWrapped("%s", context->stats.GetStateOfMind().c_str());
                    
                    ImGui::Dummy(ImVec2(0, 20));
                    
                    // Arcade Records Section
                    ImGui::TextColored(ImVec4(0, 1, 1, 1), "ARCADE RECORDS");
                    ImGui::Separator();
                    if (ImGui::BeginTable("ArcadeRecordsTable", 3, ImGuiTableFlags_NoBordersInBody)) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Storm High Score");
                        ImGui::Text("%d points", context->stats.state.arcade_records.best_storm_score);
                        
                        ImGui::TableNextColumn();
                        ImGui::TextColored(ImVec4(1, 0.6f, 0.2f, 1), "Storm Max Combo");
                        ImGui::Text("%dx", context->stats.state.arcade_records.best_storm_combo);

                        ImGui::TableNextColumn();
                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1), "Longest Streak");
                        ImGui::Text("%d points", context->stats.state.arcade_records.best_streak_score);

                        ImGui::EndTable();
                    }
                    ImGui::Dummy(ImVec2(0, 20));

                    ImGui::TextColored(ImVec4(0, 1, 1, 1), "ACTION CENTER");
                    ImGui::Separator();
                    
                    if (ImGui::Button("Train Weaknesses", ImVec2(200, 40))) {
                        context->puzzle_manager.multi_theme_filter = context->stats.GetWeakestThemes(3);
                        if (!context->puzzle_manager.multi_theme_filter.empty()) {
                            context->puzzle_manager.current_theme_filter = "Weaknesses";
                            context->puzzle_status = "Focusing on weaknesses...";
                            assets->PlaySound(assets->snd_confirm);
                        } else {
                            context->puzzle_status = "Not enough data yet.";
                            assets->PlaySound(assets->snd_wrong);
                        }
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Loads puzzles from your 3 lowest-accuracy themes.");
                    
                    ImGui::SameLine();
                    if (ImGui::Button("Reset Filters", ImVec2(120, 40))) {
                        context->puzzle_manager.current_theme_filter = "All";
                        context->puzzle_manager.multi_theme_filter.clear();
                        assets->PlaySound(assets->snd_select);
                    }
                    
                    ImGui::Dummy(ImVec2(0, 5));
                    bool pv_engine = context->play_vs_engine;
                    if (ImGui::Checkbox("Play vs Engine (Current Position)", &pv_engine)) {
                        context->play_vs_engine = pv_engine;
                        if (context->play_vs_engine) {
                            context->current_mode = GameContext::PlayMode::FREE_PLAY;
                            context->game.timer_running = false;
                            context->game.sf_active = true;
                            context->game.UpdateEngines(true);
                            assets->PlaySound(assets->snd_confirm);
                        } else {
                            assets->PlaySound(assets->snd_select);
                        }
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Turn this on to have Stockfish play out the position against you.");
                    
                    if (!context->puzzle_manager.multi_theme_filter.empty()) {
                        ImGui::TextDisabled("Active weakness filter:");
                        for (const auto& t : context->puzzle_manager.multi_theme_filter) {
                            ImGui::SameLine(); ImGui::Text("[%s]", t.c_str());
                        }
                    }

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    void DrawKeyboardInputOverlay() {
        if (!context->enable_keyboard_input) return;

        float width = 160.0f;
        float height = 36.0f;
        // Positioned just below the bottom-right corner of the board
        ImGui::SetNextWindowPos(ImVec2(last_board_pos.x + last_board_size - width, last_board_pos.y + last_board_size + 5.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0)); // Fully transparent background
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.15f, 0.7f)); // Semi-transparent frame
        
        ImGui::Begin("KeyboardInputOverlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
        
        ImGui::SetWindowFontScale(1.2f); // Slightly bigger text
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), ">");
        ImGui::SameLine(0, 4);
        
        ImGui::PushItemWidth(-1);
        
        char buf[64];
        strncpy_s(buf, sizeof(buf), keyboard_buffer.c_str(), _TRUNCATE);
        
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_AlwaysOverwrite;
        
        if (ImGui::IsWindowAppearing() || !ImGui::IsAnyItemActive()) {
             ImGui::SetKeyboardFocusHere();
        }
        
        if (ImGui::InputText("##san_input", buf, sizeof(buf), flags)) {
            std::string san = buf;
            if (!san.empty()) {
                context->HandleKeyboardInput(san);
            }
            keyboard_buffer.clear();
            ImGui::SetKeyboardFocusHere(-1); 
        } else {
            keyboard_buffer = buf;
        }
        
        ImGui::PopItemWidth();
        ImGui::End();
        
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    void DrawPlayModeHUD() {
        if (context->current_mode == GameContext::PlayMode::FREE_PLAY || context->current_mode == GameContext::PlayMode::REVIEW) return;

        float hud_height = 46.0f;
        // Moved slightly further up from the board
        ImGui::SetNextWindowPos(ImVec2(last_board_pos.x, std::max(last_board_pos.y - hud_height - 25.0f, 30.0f)), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(last_board_size, hud_height), ImGuiCond_Always);
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.1f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.3f, 0.4f, 0.5f));
        
        ImGui::Begin("PlayModeHUD", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
        
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        
        float center_y = p.y + size.y * 0.5f;

        std::string theme_str = context->puzzle_manager.current_theme_filter;
        if (theme_str == "All" || theme_str == "Weaknesses") theme_str = "";
        else {
            for (auto& c : theme_str) c = toupper(c);
            theme_str += " ";
        }

        if (context->current_mode == GameContext::PlayMode::STORM) {
            // Storm Timer Bar
            float time_ratio = std::clamp(context->storm_timer / 180.0f, 0.0f, 1.0f);
            ImU32 timer_col = IM_COL32(100, 255, 100, 255);
            if (context->storm_timer < 30.0f) timer_col = IM_COL32(255, 100, 100, 255);
            else if (context->storm_timer < 60.0f) timer_col = IM_COL32(255, 200, 50, 255);
            
            float bar_w = size.x * 0.35f;
            float bar_h = 8.0f;
            ImVec2 bar_p(p.x + 20.0f, center_y - bar_h * 0.5f + 8.0f);
            
            draw_list->AddRectFilled(bar_p, ImVec2(bar_p.x + bar_w, bar_p.y + bar_h), IM_COL32(50, 50, 50, 255), bar_h * 0.5f);
            if (time_ratio > 0) {
                draw_list->AddRectFilled(bar_p, ImVec2(bar_p.x + bar_w * time_ratio, bar_p.y + bar_h), timer_col, bar_h * 0.5f);
            }
            
            // Timer Text
            char time_buf[32]; snprintf(time_buf, 32, "%.1fs", context->storm_timer);
            ImGui::SetWindowFontScale(1.2f);
            ImVec2 time_sz = ImGui::CalcTextSize(time_buf);
            draw_list->AddText(ImVec2(bar_p.x, bar_p.y - time_sz.y - 2.0f), IM_COL32(255, 255, 255, 255), time_buf);
            ImGui::SetWindowFontScale(1.0f);

            // Combo Multiplier
            if (context->game.combo_count >= 5) {
                char combo_buf[32]; snprintf(combo_buf, 32, "+%d COMBO", context->game.combo_count);
                ImGui::SetWindowFontScale(1.1f);
                ImVec2 combo_sz = ImGui::CalcTextSize(combo_buf);
                float pulse = (sinf((float)ImGui::GetTime() * 8.0f) * 0.5f + 0.5f);
                ImU32 combo_col = IM_COL32(255, (int)(165 + pulse * 90), (int)(pulse * 100), 255);
                draw_list->AddText(ImVec2(bar_p.x + bar_w + 15.0f, center_y - combo_sz.y * 0.5f), combo_col, combo_buf);
                ImGui::SetWindowFontScale(1.0f);
            }

            // Score Text
            char score_buf[32]; snprintf(score_buf, 32, "%d", context->storm_score);
            ImGui::SetWindowFontScale(1.8f);
            ImVec2 score_sz = ImGui::CalcTextSize(score_buf);
            draw_list->AddText(ImVec2(p.x + size.x - score_sz.x - 15.0f, center_y - score_sz.y * 0.5f - 8.0f), IM_COL32(100, 200, 255, 255), score_buf);
            
            ImGui::SetWindowFontScale(0.9f);
            ImVec2 lbl_sz = ImGui::CalcTextSize((theme_str + "SCORE").c_str());
            draw_list->AddText(ImVec2(p.x + size.x - score_sz.x - lbl_sz.x - 25.0f, center_y - lbl_sz.y * 0.5f - 8.0f), IM_COL32(150, 150, 150, 255), (theme_str + "SCORE").c_str());

            // PB
            char pb_buf[32]; snprintf(pb_buf, 32, "PB: %d", context->stats.state.arcade_records.best_storm_score);
            ImGui::SetWindowFontScale(0.8f);
            ImVec2 pb_sz = ImGui::CalcTextSize(pb_buf);
            draw_list->AddText(ImVec2(p.x + size.x - pb_sz.x - 15.0f, center_y + score_sz.y * 0.5f - 6.0f), IM_COL32(100, 100, 100, 255), pb_buf);

        } else if (context->current_mode == GameContext::PlayMode::STREAK) {
            // Heart drawing lambda
            auto DrawHeart = [&](ImVec2 center, float s, ImU32 fill_col, ImU32 outline_col) {
                auto DrawShape = [&](float scale, ImU32 col) {
                    float r = scale * 0.28f;
                    float dx = scale * 0.25f;
                    float dy = scale * 0.15f;
                    ImVec2 c1(center.x - dx, center.y - dy);
                    ImVec2 c2(center.x + dx, center.y - dy);
                    ImVec2 bottom(center.x, center.y + scale * 0.45f);
                    
                    ImVec2 p1(c1.x - r * 0.85f, c1.y + r * 0.5f);
                    ImVec2 p2(c2.x + r * 0.85f, c2.y + r * 0.5f);
                    
                    draw_list->AddCircleFilled(c1, r, col, 20);
                    draw_list->AddCircleFilled(c2, r, col, 20);
                    draw_list->AddTriangleFilled(p1, p2, bottom, col);
                    draw_list->AddTriangleFilled(c1, c2, bottom, col);
                };
                
                if (outline_col != 0) DrawShape(s + 3.0f, outline_col);
                DrawShape(s, fill_col);
            };

            float start_x = p.x + 20.0f;
            
            ImGui::SetWindowFontScale(1.0f);
            ImVec2 lives_lbl_sz = ImGui::CalcTextSize("LIVES");
            draw_list->AddText(ImVec2(start_x, center_y - lives_lbl_sz.y * 0.5f), IM_COL32(150, 150, 150, 255), "LIVES");

            float hearts_start_x = start_x + lives_lbl_sz.x + 25.0f;
            float heart_size = 20.0f;
            float spacing = 28.0f;
            
            float pulse = (sinf((float)ImGui::GetTime() * 5.0f) * 0.5f + 0.5f);

            for (int i = 0; i < 3; i++) {
                ImVec2 h_pos(hearts_start_x + i * spacing, center_y);
                if (i < context->streak_lives) {
                    float s = heart_size + (i == context->streak_lives - 1 && context->streak_lives == 1 ? pulse * 3.0f : 0.0f);
                    DrawHeart(h_pos, s, IM_COL32(255, 60, 60, 255), IM_COL32(255, 255, 255, 100));
                } else {
                    DrawHeart(h_pos, heart_size, IM_COL32(60, 60, 60, 150), IM_COL32(100, 100, 100, 150));
                }
            }

            // Score Text
            char score_buf[32]; snprintf(score_buf, 32, "%d", context->streak_score);
            ImGui::SetWindowFontScale(1.8f);
            ImVec2 score_sz = ImGui::CalcTextSize(score_buf);
            draw_list->AddText(ImVec2(p.x + size.x - score_sz.x - 15.0f, center_y - score_sz.y * 0.5f - 8.0f), IM_COL32(100, 255, 100, 255), score_buf);
            
            ImGui::SetWindowFontScale(0.9f);
            ImVec2 lbl_sz = ImGui::CalcTextSize((theme_str + "STREAK").c_str());
            draw_list->AddText(ImVec2(p.x + size.x - score_sz.x - lbl_sz.x - 25.0f, center_y - lbl_sz.y * 0.5f - 8.0f), IM_COL32(150, 150, 150, 255), (theme_str + "STREAK").c_str());

            // PB
            char pb_buf[32]; snprintf(pb_buf, 32, "PB: %d", context->stats.state.arcade_records.best_streak_score);
            ImGui::SetWindowFontScale(0.8f);
            ImVec2 pb_sz = ImGui::CalcTextSize(pb_buf);
            draw_list->AddText(ImVec2(p.x + size.x - pb_sz.x - 15.0f, center_y + score_sz.y * 0.5f - 6.0f), IM_COL32(100, 100, 100, 255), pb_buf);
        }
        
        ImGui::SetWindowFontScale(1.0f);
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);

        if (context->mode_game_over) {
            ImGui::OpenPopup("Game Over##Mode");
        }
        if (ImGui::BeginPopupModal("Game Over##Mode", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
            ImGui::Text("Run Finished!");
            ImGui::Separator();
            if (context->current_mode == GameContext::PlayMode::STORM) {
                ImGui::Text("Final Score: %d", context->storm_score);
            } else if (context->current_mode == GameContext::PlayMode::STREAK) {
                ImGui::Text("Final Streak: %d", context->streak_score);
            }
            ImGui::Separator();
            if (ImGui::Button("Play Again", ImVec2(120, 0))) {
                if (context->current_mode == GameContext::PlayMode::STORM) {
                    context->storm_timer = 180.0f; context->storm_score = 0; context->mode_game_over = false;
                    context->StartPuzzle(context->puzzle_manager.GetProgressivePuzzle(0));
                } else {
                    context->streak_score = 0; context->streak_lives = 3; context->mode_game_over = false;
                    context->StartPuzzle(context->puzzle_manager.GetProgressivePuzzle(0));
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Exit", ImVec2(120, 0))) {
                context->current_mode = GameContext::PlayMode::FREE_PLAY;
                context->mode_game_over = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void DrawEvalBars(ImVec2 board_tr, float board_size) {
        if (!settings.state.show_eval_bar_sf && !settings.state.show_eval_bar_lc0) return;

        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
        
        float bar_width = 18.0f;
        float padding = 6.0f;
        
        ImVec2 current_pos = ImVec2(board_tr.x + padding, board_tr.y);

        auto draw_bar = [&](EngineManager& eng, ImU32 bar_color) {
            if (!eng.IsRunning()) return;
            auto info = eng.GetInfo();
            float eval = 0.0f;
            if (!info.lines.empty()) {
                auto best_line = info.lines.begin()->second;
                if (best_line.score_mate != 0) {
                    eval = best_line.score_mate > 0 ? 10.0f : -10.0f;
                } else {
                    eval = best_line.score_cp / 100.0f;
                }
                // Absolute evaluation from white's perspective
                if (context->game.board.sideToMove() == chess::Color::BLACK) {
                    eval = -eval; 
                }
            }
            
            // Map eval to percentage (0.0 to 1.0, where 0.5 is 0.0 eval)
            // Using arctan mapping for a smooth curve
            float fill_pct = 0.5f + 0.5f * (2.0f / 3.14159f) * atanf(eval / 1.5f);
            
            if (context->game.is_flipped) {
                fill_pct = 1.0f - fill_pct;
            }
            
            ImVec2 bar_min = current_pos;
            ImVec2 bar_max = ImVec2(current_pos.x + bar_width, current_pos.y + board_size);
            
            // Draw background (Dark for Black advantage)
            draw_list->AddRectFilled(bar_min, bar_max, IM_COL32(30, 30, 30, 220), 3.0f);
            
            // Draw foreground (Light for White advantage)
            float white_height = board_size * fill_pct;
            ImVec2 white_min = ImVec2(current_pos.x, bar_max.y - white_height);
            draw_list->AddRectFilled(white_min, bar_max, IM_COL32(235, 235, 235, 240), 3.0f);
            
            // Draw colored outline for the engine
            draw_list->AddRect(bar_min, bar_max, bar_color, 3.0f, 0, 2.0f);
            
            // Draw text evaluation
            std::string eval_str = "";
            if (!info.lines.empty()) {
                auto best_line = info.lines.begin()->second;
                if (best_line.score_mate != 0) eval_str = "M" + std::to_string(std::abs(best_line.score_mate));
                else {
                    float display_eval = best_line.score_cp / 100.0f;
                    if (context->game.board.sideToMove() == chess::Color::BLACK) display_eval = -display_eval;
                    std::stringstream ss;
                    ss << std::fixed << std::setprecision(1) << std::abs(display_eval);
                    eval_str = ss.str();
                }
            }
            
            if (!eval_str.empty()) {
                ImVec2 txt_sz = ImGui::CalcTextSize(eval_str.c_str());
                // Determine if text goes at top (if white is winning) or bottom
                float text_y = (fill_pct > 0.5f) ? bar_min.y + 5.0f : bar_max.y - txt_sz.y - 5.0f;
                // If the bar is mostly white, text color should be dark
                ImU32 text_col = (fill_pct > 0.5f) ? IM_COL32(0, 0, 0, 255) : IM_COL32(255, 255, 255, 255);
                
                draw_list->AddText(ImVec2(current_pos.x + (bar_width - txt_sz.x) * 0.5f, text_y), text_col, eval_str.c_str());
            }

            current_pos.x += bar_width + padding;
        };

        if (settings.state.show_eval_bar_sf) {
            draw_bar(context->game.sf_engine, IM_COL32(0, 150, 255, 200)); // Blue for SF
        }
        if (settings.state.show_eval_bar_lc0) {
            draw_bar(context->game.lc0_engine, IM_COL32(255, 150, 0, 200)); // Orange for Lc0
        }
    }

    void Render() {
        float dt = ImGui::GetIO().DeltaTime;
        combo_inactivity_timer += dt;

        if (context->game.combo_count != last_combo_count) {
             if (context->game.combo_count > last_combo_count) { 
                 combo_pop_timer = 0.5f; 
                 combo_inactivity_timer = 0.0f; 
                 
                 // Spawn embers around where the combo text is drawn
                 ImVec2 ember_pos(last_board_pos.x + last_board_size * 0.5f - 100.0f, last_board_pos.y - 80.0f);
                 ImU32 col = IM_COL32(255, 200, 50, 255);
                 if (context->game.combo_count >= 20) col = IM_COL32(255, 100, 50, 255);
                 if (context->game.combo_count >= 40) col = IM_COL32(255, 50, 255, 255);
                 context->game.effects.SpawnComboTextEmbers(ember_pos, ImVec2(200, 40), col);
             }
             last_combo_count = context->game.combo_count;
        }
        if (combo_pop_timer > 0.0f) combo_pop_timer -= dt;

        if (context->game.flip_val != last_flip_val) {
            float angle = context->game.flip_val * 3.14159265f;
            cached_sin = sinf(angle); cached_cos = cosf(angle);
            last_flip_val = context->game.flip_val;
        }

        if (theme_dirty) { ReloadTheme(); theme_dirty = false; }

        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplSDL3_NewFrame(); ImGui::NewFrame();
        
        DrawAppBackground();
        context->game.effects.DrawComboEffects(ImGui::GetBackgroundDrawList(), last_board_pos, last_board_size, context->game.combo_count);

        DrawComboCounter(); DrawMainMenuBar(); DrawHelpModal(); DrawStatsWindow(); DrawPerformanceAnalyticsWindow(); DrawAICoach(); DrawEngineTab(); DrawMoveList(); DrawBatchAnalysisDashboard(); DrawPlayModeHUD(); DrawKeyboardInputOverlay();

        ImGuiIO& io = ImGui::GetIO();

        if (!io.WantTextInput) { 
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) context->game.go_back();
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
                int old_node = context->game.current_node_idx;
                context->game.go_forward();
                if (context->game.current_node_idx != old_node) {
                    context->TriggerAnimation(context->game.move_tree[context->game.current_node_idx].move);
                }
            }
            
            if (!context->enable_keyboard_input) {
                if (ImGui::IsKeyPressed(ImGuiKey_F)) { context->game.is_flipped = !context->game.is_flipped; assets->PlaySound(assets->snd_select); }
                if (ImGui::IsKeyPressed(ImGuiKey_A)) { context->game.auto_next = !context->game.auto_next; assets->PlaySound(assets->snd_select); }
                if (ImGui::IsKeyPressed(ImGuiKey_B)) { context->blindfold_active = !context->blindfold_active; if (context->blindfold_active) context->current_blindfold_timer = context->blindfold_use_timer ? context->blindfold_delay : 0.0f; assets->PlaySound(assets->snd_select); }
                if (ImGui::IsKeyPressed(ImGuiKey_R)) { if (context->puzzle_manager.GetCurrentPuzzle()) context->StartPuzzle(context->puzzle_manager.GetCurrentPuzzle()); else context->game.reset(); assets->PlaySound(assets->snd_confirm); }
                if (ImGui::IsKeyPressed(ImGuiKey_N)) { context->game.review_mode = false; context->StartPuzzle(context->puzzle_manager.GetRandomPuzzle()); assets->PlaySound(assets->snd_confirm); }
                if (ImGui::IsKeyPressed(ImGuiKey_Z)) { context->game.focus_mode = !context->game.focus_mode; assets->PlaySound(assets->snd_select); }
                if (ImGui::IsKeyPressed(ImGuiKey_H)) { show_help = !show_help; assets->PlaySound(assets->snd_select); }
                if (ImGui::IsKeyPressed(ImGuiKey_T)) { if (io.KeyCtrl) show_title_bars = !show_title_bars; else settings.state.show_timer = !settings.state.show_timer; assets->PlaySound(assets->snd_select); }
                if (ImGui::IsKeyPressed(ImGuiKey_E)) { settings.state.show_engine_tab = !settings.state.show_engine_tab; assets->PlaySound(assets->snd_select); }
                if (ImGui::IsKeyPressed(ImGuiKey_S)) { context->game.sf_active = !context->game.sf_active; context->game.UpdateEngines(true); if (context->game.sf_active) settings.state.show_engine_tab = true; assets->PlaySound(assets->snd_select); }
                if (ImGui::IsKeyPressed(ImGuiKey_L)) { context->game.lc0_active = !context->game.lc0_active; context->game.UpdateEngines(true); if (context->game.lc0_active) settings.state.show_engine_tab = true; assets->PlaySound(assets->snd_select); }
                if (ImGui::IsKeyPressed(ImGuiKey_P)) { if (context->game.preview_mode) { context->game.preview_mode = false; context->game.preview_animating = false; context->game.preview_moves.clear(); assets->PlaySound(assets->snd_select); } }
            }
        }
        
        if (!io.WantCaptureMouse || ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {
             if (io.MouseWheel < 0) {
                 int old_node = context->game.current_node_idx;
                 context->game.go_forward();
                 if (context->game.current_node_idx != old_node) {
                     context->TriggerAnimation(context->game.move_tree[context->game.current_node_idx].move);
                 }
             } 
             else if (io.MouseWheel > 0) context->game.go_back();
        }

        ImGuiWindowFlags panel_flags = ImGuiWindowFlags_None;
        if (!show_title_bars) panel_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        ImGuiWindowFlags board_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        if (!show_title_bars) board_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize;

        if (reset_layout) ImGui::SetNextWindowPos(ImVec2(20, 30), ImGuiCond_Always);
        else ImGui::SetNextWindowPos(ImVec2(20, 30), ImGuiCond_FirstUseEver);

        if (show_title_bars) {
            ImGui::SetNextWindowSizeConstraints(ImVec2(100, 100), ImVec2(FLT_MAX, FLT_MAX), [](ImGuiSizeCallbackData* data) {
                float decoration_h = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
                float board_sz = std::max(data->DesiredSize.x, data->DesiredSize.y - decoration_h);
                data->DesiredSize.x = board_sz; data->DesiredSize.y = board_sz + decoration_h;
            });
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("Chess Board", nullptr, board_flags);
        
        if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            ImVec2 pos = ImGui::GetWindowPos(); ImVec2 delta = ImGui::GetIO().MouseDelta; ImGui::SetWindowPos(ImVec2(pos.x + delta.x, pos.y + delta.y));
        }
        if (show_title_bars) {
            ImVec2 avail = ImGui::GetContentRegionAvail(); float new_board_size = std::min(avail.x, avail.y);
            if (new_board_size > 100.0f && std::abs(new_board_size - BOARD_SIZE) > 1.0f) { BOARD_SIZE = new_board_size; SQUARE_SIZE = BOARD_SIZE / 8.0f; }
        }

        DrawChessBoard();
        ImGui::End();
        ImGui::PopStyleVar(2);

        // Draw eval bars outside the main board window
        DrawEvalBars(ImVec2(last_board_pos.x + last_board_size, last_board_pos.y), last_board_size);

        if (settings.state.show_controls && !context->game.focus_mode) {
            if (reset_layout) ImGui::SetNextWindowPos(ImVec2(BOARD_SIZE + 60, 30), ImGuiCond_Always);
            else ImGui::SetNextWindowPos(ImVec2(BOARD_SIZE + 60, 30), ImGuiCond_FirstUseEver);
            ImGui::Begin("Dashboard", &settings.state.show_controls, panel_flags);
            
            ImGui::SeparatorText("SESSION");
            ImGui::TextColored(context->status_color, "%s", context->puzzle_status.c_str());
            
            if (settings.state.show_side_to_move) {
                chess::Board temp_board(context->game.start_fen);
                auto line = context->game.get_current_line_indices();
                for (int idx : line) temp_board.makeMove(context->game.move_tree[idx].move);
                bool is_white = (temp_board.sideToMove() == chess::Color::WHITE);
                ImGui::TextColored(is_white ? ImVec4(1,1,1,1) : ImVec4(0.5f, 0.5f, 0.5f, 1), is_white ? "White to Move" : "Black to Move");
            }

            if (settings.state.show_timer) {
                if (context->game.timer_running) ImGui::TextColored(ImVec4(0,1,1,1), "Time: %.1f s", context->game.puzzle_timer);
                else {
                    if (context->game.was_solved) ImGui::Text("Time: %.1f s (%.1f s Total)", context->game.last_solve_time, context->game.last_total_time);
                    else ImGui::Text("Time: %.1f s", context->game.puzzle_timer);
                }
            }

            float progress = 0.0f; 
            auto main_branch = context->game.get_main_branch_indices();
            if (!main_branch.empty()) progress = (float)context->game.get_current_line_indices().size() / (float)main_branch.size();
            ImGui::ProgressBar(progress, ImVec2(-1, 6), ""); ImGui::Dummy(ImVec2(0, 10)); 
            
            ImGui::SeparatorText("PUZZLES");
            static int selected_rating = 2000;
            if (ImGui::BeginTable("PuzzleSettings", 2)) {
                ImGui::TableNextColumn(); ImGui::Text("Rating"); ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
                if (ImGui::BeginCombo("##rating", (std::to_string(selected_rating) + "+").c_str())) {
                    for (int r = 2000; r <= 3200; r += 100) { if (ImGui::Selectable((std::to_string(r) + "+").c_str(), selected_rating == r)) { selected_rating = r; assets->PlaySound(assets->snd_select); } }
                    ImGui::EndCombo();
                }
                ImGui::TableNextColumn(); ImGui::Text("Theme"); ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
                if (ImGui::BeginCombo("##theme", context->puzzle_manager.current_theme_filter.c_str())) {
                    for (const auto& theme : context->puzzle_manager.available_themes) { if (ImGui::Selectable(theme.c_str(), context->puzzle_manager.current_theme_filter == theme)) { context->puzzle_manager.current_theme_filter = theme; assets->PlaySound(assets->snd_select); } }
                    ImGui::EndCombo();
                }
                ImGui::EndTable();
            }

            ImGui::Spacing();
            if (ImGui::Button("Load Set", ImVec2(ImGui::GetContentRegionAvail().x * 0.3f, 0))) {
                std::string path = "Assets/data/puzzles_" + std::to_string(selected_rating) + ".json";
                if (!context->puzzle_manager.LoadPuzzleSet(path)) { path = "../../" + path; context->puzzle_manager.LoadPuzzleSet(path); }
                context->game.review_mode = false; assets->PlaySound(assets->snd_confirm);
            }
            ImGui::SameLine();
            if (ImGui::Button("Hint", ImVec2(ImGui::GetContentRegionAvail().x * 0.45f, 0))) { context->game.hint_level = (context->game.hint_level + 1) % 3; assets->PlaySound(assets->snd_select); }
            ImGui::SameLine();
            if (ImGui::Button("Next", ImVec2(-1, 0))) { context->game.review_mode = false; context->StartPuzzle(context->puzzle_manager.GetRandomPuzzle()); assets->PlaySound(assets->snd_confirm); }
            ImGui::TextDisabled("%d active puzzles loaded", (int)context->puzzle_manager.active_puzzles.size()); ImGui::Dummy(ImVec2(0, 10));

            if (settings.state.show_system_usage) {
                ImGui::SeparatorText("SYSTEM"); ImGui::Text("CPU: %.1f%%", context->sys_monitor.cpuUsage); ImGui::ProgressBar((float)context->sys_monitor.cpuUsage / 100.0f, ImVec2(-1, 6), "");
                if (context->sys_monitor.gpuUsages.empty()) ImGui::TextDisabled("GPU: N/A");
                else { for (auto const& [id, usage] : context->sys_monitor.gpuUsages) { ImGui::Text("GPU %d: %.1f%%", id, usage); ImGui::ProgressBar((float)usage / 100.0f, ImVec2(-1, 6), ""); } }
                ImGui::Dummy(ImVec2(0, 10));
            }

            if (settings.state.show_dashboard_engine_stats) {
                ImGui::SeparatorText("ENGINE ANALYSIS");
                if (ImGui::BeginTable("EngineStats", 2, ImGuiTableFlags_BordersInnerV)) {
                    ImGui::TableNextColumn(); ImGui::Text("Stockfish"); ImGui::TableNextColumn(); 
                    if (context->game.sf_active) ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.6f, 1.0f), "Active"); else ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Off");
                    ImGui::TableNextColumn(); ImGui::Text("Lc0"); ImGui::TableNextColumn(); 
                    if (context->game.lc0_active) ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Active"); else ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Off");
                    ImGui::EndTable(); ImGui::Dummy(ImVec2(0, 10));
                }
            }
            if (Puzzle* p = context->puzzle_manager.GetCurrentPuzzle()) {
                ImGui::SeparatorText("PUZZLE INFO");
                if (ImGui::BeginTable("PuzzleInfo", 2)) {
                    ImGui::TableNextColumn(); ImGui::Text("ID:"); ImGui::TableNextColumn(); ImGui::Text("%s", p->id.c_str());
                    ImGui::TableNextColumn(); ImGui::Text("Rating:"); ImGui::TableNextColumn(); ImGui::Text("%s", p->rating.c_str());
                    ImGui::TableNextColumn(); ImGui::Text("SRS Stage:"); ImGui::TableNextColumn(); ImGui::Text("%d", p->srs_stage);
                    ImGui::EndTable();
                }
                if (context->game.review_mode) ImGui::TextColored(ImVec4(1,1,0,1), "[Review Mode]");
                if (ImGui::CollapsingHeader("Themes")) ImGui::TextWrapped("%s", p->themes.c_str());
            }
            ImGui::End();
        }

        if (settings.state.show_history && !context->game.focus_mode) {
            if (reset_layout) { ImGui::SetNextWindowPos(ImVec2(980, 30), ImGuiCond_Always); ImGui::SetNextWindowSize(ImVec2(280, 680), ImGuiCond_Always); }
            else ImGui::SetNextWindowPos(ImVec2(980, 30), ImGuiCond_FirstUseEver);
            ImGui::Begin("History", &settings.state.show_history);
            RenderHistoryList();
            ImGui::End();
        }

        if (reset_layout) reset_layout = false;

        if (context->game.auto_play_active && context->game.opening_notif_active && context->game.opening_notif_timer > 0.0f) {
            ImDrawList* draw_list = ImGui::GetForegroundDrawList();
            ImVec2 center = ImVec2(last_board_pos.x + last_board_size * 0.5f, last_board_pos.y + last_board_size * 0.5f);
            
            ImFont* font = ImGui::GetFont();
            float font_size = font->FontSize * 1.5f;
            ImVec2 text_sz = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, context->game.opening_notif_text.c_str());
            
            float padding = 20.0f;
            ImVec2 p_min(center.x - text_sz.x * 0.5f - padding, center.y - text_sz.y * 0.5f - padding);
            ImVec2 p_max(center.x + text_sz.x * 0.5f + padding, center.y + text_sz.y * 0.5f + padding);
            
            // Fade out effect
            float alpha = std::min(1.0f, context->game.opening_notif_timer);
            ImU32 bg_col = IM_COL32(20, 20, 30, (int)(220 * alpha));
            ImU32 text_col = IM_COL32(255, 255, 255, (int)(255 * alpha));
            ImU32 border_col = IM_COL32(100, 150, 255, (int)(200 * alpha));

            draw_list->AddRectFilled(p_min, p_max, bg_col, 8.0f);
            draw_list->AddRect(p_min, p_max, border_col, 8.0f, 0, 2.0f);
            
            ImVec2 text_pos(center.x - text_sz.x * 0.5f, center.y - text_sz.y * 0.5f);
            draw_list->AddText(font, font_size, text_pos, text_col, context->game.opening_notif_text.c_str(), NULL);
        }

        if (context->game.banner_active && context->game.banner_timer > 0.0f) {
            ImDrawList* draw_list = ImGui::GetForegroundDrawList();
            float banner_w = last_board_size * 0.8f;
            float banner_h = 100.0f;
            ImVec2 center = ImVec2(last_board_pos.x + last_board_size * 0.5f, last_board_pos.y + last_board_size * 0.5f);
            
            // Dim background
            draw_list->AddRectFilled(last_board_pos, ImVec2(last_board_pos.x + last_board_size, last_board_pos.y + last_board_size), IM_COL32(0, 0, 0, 150));
            
            // Banner Rect
            ImVec2 p_min(center.x - banner_w * 0.5f, center.y - banner_h * 0.5f);
            ImVec2 p_max(center.x + banner_w * 0.5f, center.y + banner_h * 0.5f);
            draw_list->AddRectFilled(p_min, p_max, IM_COL32(20, 20, 25, 230), 10.0f);
            
            // Animated Border
            ImU32 border_col = ImGui::ColorConvertFloat4ToU32(context->game.banner_color);
            float border_thickness = 3.0f + 1.5f * sinf(context->game.banner_timer * 5.0f);
            draw_list->AddRect(p_min, p_max, border_col, 10.0f, 0, border_thickness);
            
            // Banner Text
            ImFont* font = ImGui::GetFont();
            float font_size = font->FontSize * 2.0f;
            ImVec2 text_sz = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, context->game.banner_text.c_str());
            ImVec2 text_pos(center.x - text_sz.x * 0.5f, center.y - text_sz.y * 0.5f);
            draw_list->AddText(font, font_size, text_pos, border_col, context->game.banner_text.c_str(), NULL);
        }

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        if (settings.state.current_app_theme == 3) glClearColor(0.94f, 0.96f, 1.00f, 1.00f); else if (settings.state.current_app_theme == 4) glClearColor(0.0f, 0.0f, 0.0f, 1.00f); else glClearColor(0.12f, 0.12f, 0.14f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); SDL_GL_SwapWindow(window);
    }
};