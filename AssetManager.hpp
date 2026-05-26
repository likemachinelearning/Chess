#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <filesystem>
#include <algorithm>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3_image/SDL_image.h>

// --- Audio Types ---
struct SoundEffect {
    Uint8* buffer = nullptr;
    Uint32 length = 0;
    SDL_AudioSpec spec = {}; 
};

// --- Asset Manager Class ---
class AssetManager {
public:
    // Audio State
    SDL_AudioStream* audio_stream = nullptr;
    SDL_AudioStream* tick_stream = nullptr;
    SoundEffect snd_move, snd_capture, snd_correct, snd_wrong, snd_check, snd_checkmate, snd_select, snd_confirm, snd_startup, snd_tick, snd_shatter;

    // --- Helper: Load Texture ---
    static GLuint LoadTextureFromFile(const char* filename) {
        if (!filename || strlen(filename) == 0) return 0;
        
        std::string path = filename;
        bool found = false;

        // 1. Try absolute or current path
        if (std::filesystem::exists(path)) {
            found = true;
        } else {
            // 2. Try several fallback depths
            std::string fallbacks[] = {
                std::string("../") + filename,
                std::string("../../") + filename,
                std::string("../../../") + filename,
                // 3. If filename already has ../ prefix, try stripping it too
                (path.substr(0, 3) == "../") ? path.substr(3) : ""
            };

            for (const auto& fb : fallbacks) {
                if (!fb.empty() && std::filesystem::exists(fb)) {
                    path = fb;
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            SDL_Log("Could not find file on disk: %s", filename);
            return 0;
        }

        SDL_Surface* surface = IMG_Load(path.c_str());
        if (!surface) {
            SDL_Log("IMG_Load failed for %s: %s", path.c_str(), SDL_GetError());
            return 0;
        }

        if (surface->format != SDL_PIXELFORMAT_RGBA32) {
            SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
            SDL_DestroySurface(surface);
            surface = converted;
        }

        GLuint texture_id;
        glGenTextures(1, &texture_id);
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
        SDL_DestroySurface(surface);
        return texture_id;
    }

    // --- Inner Struct: ThemeManager ---
    struct ThemeManager {
        std::string current_piece_set = "gioco";
        std::string current_board_theme = "Default";
        std::vector<std::string> available_piece_sets;
        std::vector<std::string> available_board_themes;
        std::vector<std::string> available_fonts;
        
        char custom_piece_path[256] = "";
        char custom_board_path[256] = "";
        
        std::string current_font_file = "Roboto-Regular.ttf";
        int current_font_size = 18;
        
        GLuint piece_textures[16] = {0};
        GLuint board_texture = 0;
        GLuint bg_texture = 0;
        
        char custom_bg_path[256] = "";
        float bg_brightness = 0.5f;
        float bg_scale = 1.0f;
        bool bg_enabled = true;

        // New Members for Settings Compatibility
        int current_theme_idx = 0; 
        int current_piece_theme_idx = 0;
        int current_board_theme_idx = 0;
        float global_font_scale = 1.0f;
        
        void SetPieceTheme(int idx) {
            if (idx >= 0 && idx < (int)available_piece_sets.size()) {
                current_piece_theme_idx = idx;
                LoadPieceSet(available_piece_sets[idx]);
            }
        }

        void SetBoardTheme(int idx) {
            if (idx >= 0 && idx < (int)available_board_themes.size()) {
                current_board_theme_idx = idx;
                LoadBoardTheme(available_board_themes[idx]);
            }
        }

        void SyncThemesFromIndices() {
            if (current_piece_theme_idx >= 0 && current_piece_theme_idx < (int)available_piece_sets.size()) {
                current_piece_set = available_piece_sets[current_piece_theme_idx];
            }
            if (current_board_theme_idx >= 0 && current_board_theme_idx < (int)available_board_themes.size()) {
                current_board_theme = available_board_themes[current_board_theme_idx];
            }
        }
        
        void ScanAssets() {
            available_piece_sets.clear();
            available_board_themes.clear();
            available_board_themes.push_back("Default");
            available_fonts.clear();
            
            // Scan Pieces
            std::string piece_root = "Assets/Piece";
            if (!std::filesystem::exists(piece_root)) piece_root = "../../Assets/Piece";
            if (std::filesystem::exists(piece_root)) {
                for (const auto& entry : std::filesystem::directory_iterator(piece_root)) {
                    if (entry.is_directory()) {
                        available_piece_sets.push_back(entry.path().filename().string());
                    }
                }
            }
            std::sort(available_piece_sets.begin(), available_piece_sets.end());
            
            // Scan Boards
            std::string board_root = "Assets/Board";
            if (!std::filesystem::exists(board_root)) board_root = "../../Assets/Board";
            if (std::filesystem::exists(board_root)) {
                for (const auto& entry : std::filesystem::directory_iterator(board_root)) {
                    if (entry.is_regular_file()) {
                        available_board_themes.push_back(entry.path().filename().string());
                    }
                }
            }
            std::sort(available_board_themes.begin(), available_board_themes.end());
            
            // Scan Fonts
            std::string font_root = "Assets/fonts";
            if (!std::filesystem::exists(font_root)) font_root = "../../Assets/fonts";
            if (std::filesystem::exists(font_root)) {
                for (const auto& entry : std::filesystem::directory_iterator(font_root)) {
                    if (entry.is_regular_file()) {
                        std::string ext = entry.path().extension().string();
                        if (ext == ".ttf" || ext == ".otf") {
                            available_fonts.push_back(entry.path().filename().string());
                        }
                    }
                }
            }
            std::sort(available_fonts.begin(), available_fonts.end());
        }

        void LoadPieceSet(const std::string& set_name, bool is_custom = false) {
            // Clear old textures
            for (int i = 0; i < 16; ++i) {
                if (piece_textures[i] != 0) glDeleteTextures(1, &piece_textures[i]);
                piece_textures[i] = 0;
            }

            std::string base_path;
            if (is_custom) {
                base_path = set_name;
            } else {
                 base_path = "Assets/Piece/" + set_name;
                 if (!std::filesystem::exists(base_path)) base_path = "../../Assets/Piece/" + set_name;
            }
            
            const char* pieces[] = { "wP", "wN", "wB", "wR", "wQ", "wK", "bP", "bN", "bB", "bR", "bQ", "bK" };
            int internal_indices[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
            const char* exts[] = { ".svg", ".png" };
            
            for (int i = 0; i < 12; ++i) {
                std::string filename;
                for (const char* ext : exts) {
                    std::string path = base_path + "/" + pieces[i] + ext;
                    if (std::filesystem::exists(path)) {
                        filename = path;
                        break;
                    }
                }
                
                if (!filename.empty()) {
                    piece_textures[internal_indices[i]] = AssetManager::LoadTextureFromFile(filename.c_str());
                }
            }
            if (!is_custom) current_piece_set = set_name;
        }

        bool ImportPieceSet(const std::string& source_path) {
            try {
                std::filesystem::path src(source_path);
                if (!std::filesystem::exists(src) || !std::filesystem::is_directory(src)) return false;

                std::string name = src.filename().string();
                std::string dest_root = "Assets/Piece";
                if (!std::filesystem::exists(dest_root)) dest_root = "../../Assets/Piece";
                if (!std::filesystem::exists(dest_root)) return false;

                std::filesystem::path dest = std::filesystem::path(dest_root) / name;
                
                // Copy options: overwrite existing
                std::filesystem::copy(src, dest, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
                
                ScanAssets();
                return true;
            } catch (...) {
                return false;
            }
        }

        bool DeletePieceSet(const std::string& name) {
            if (name == "gioco" || name == "Default") return false; // Protected
            try {
                std::string root = "Assets/Piece";
                if (!std::filesystem::exists(root)) root = "../../Assets/Piece";
                
                std::filesystem::path p = std::filesystem::path(root) / name;
                if (std::filesystem::exists(p)) {
                    std::filesystem::remove_all(p);
                    ScanAssets();
                    return true;
                }
            } catch (...) {}
            return false;
        }

        bool ImportBoardTheme(const std::string& source_path) {
            try {
                std::filesystem::path src(source_path);
                if (!std::filesystem::exists(src) || !std::filesystem::is_regular_file(src)) return false;

                std::string name = src.filename().string();
                std::string dest_root = "Assets/Board";
                if (!std::filesystem::exists(dest_root)) dest_root = "../../Assets/Board";
                if (!std::filesystem::exists(dest_root)) return false;

                std::filesystem::path dest = std::filesystem::path(dest_root) / name;
                std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing);
                
                ScanAssets();
                return true;
            } catch (...) {
                return false;
            }
        }

        bool DeleteBoardTheme(const std::string& name) {
            if (name == "Default") return false;
            try {
                std::string root = "Assets/Board";
                if (!std::filesystem::exists(root)) root = "../../Assets/Board";
                
                std::filesystem::path p = std::filesystem::path(root) / name;
                if (std::filesystem::exists(p)) {
                    std::filesystem::remove(p);
                    ScanAssets();
                    return true;
                }
            } catch (...) {}
            return false;
        }
        
        void LoadBoardTheme(const std::string& theme_name, bool is_custom = false) {
            if (board_texture != 0) {
                glDeleteTextures(1, &board_texture);
                board_texture = 0;
            }
            
            if (theme_name == "Default" && !is_custom) {
                current_board_theme = "Default";
                return;
            }
            
            std::string path;
            if (is_custom) {
                path = theme_name;
            } else {
                path = "Assets/Board/" + theme_name;
                if (!std::filesystem::exists(path)) path = "../../Assets/Board/" + theme_name;
            }
            
            if (std::filesystem::exists(path)) {
                 board_texture = AssetManager::LoadTextureFromFile(path.c_str());
                 if (!is_custom) current_board_theme = theme_name;
            }
        }
    };

    ThemeManager theme_manager;

    // --- Audio Logic ---
    float master_volume = 1.0f;
    bool enable_sound = true;

    void SetVolume(float vol) {
        master_volume = std::clamp(vol, 0.0f, 1.0f);
        if (audio_stream) {
            SDL_SetAudioStreamGain(audio_stream, master_volume);
        }
    }

    bool LoadWAV(const char* filename, SoundEffect& snd) {
        std::string path = filename;
        if (!SDL_LoadWAV(path.c_str(), &snd.spec, &snd.buffer, &snd.length)) {
            path = std::string("../../") + filename;
            if (!SDL_LoadWAV(path.c_str(), &snd.spec, &snd.buffer, &snd.length)) {
                SDL_Log("Failed to load WAV: %s", filename);
                return false;
            }
        }
        return true;
    }

    void PlaySound(const SoundEffect& snd, float pitch = 1.0f) {
        if (audio_stream && snd.buffer) {
            SDL_ClearAudioStream(audio_stream);
            SDL_SetAudioStreamFrequencyRatio(audio_stream, pitch);
            if (SDL_SetAudioStreamFormat(audio_stream, &snd.spec, nullptr)) {
                SDL_PutAudioStreamData(audio_stream, snd.buffer, snd.length);
            } else {
                SDL_Log("Failed to set stream format: %s", SDL_GetError());
            }
        }
    }

    void PlayTickSound() {
        if (tick_stream && snd_tick.buffer) {
            SDL_ClearAudioStream(tick_stream);
            SDL_SetAudioStreamFrequencyRatio(tick_stream, 1.0f);
            if (SDL_SetAudioStreamFormat(tick_stream, &snd_tick.spec, nullptr)) {
                SDL_PutAudioStreamData(tick_stream, snd_tick.buffer, snd_tick.length);
            }
        }
    }

    void InitAudio() {
        audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr, nullptr, nullptr);
        if (audio_stream) {
            SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(audio_stream));
            SetVolume(master_volume);
        } else {
            SDL_Log("Failed to open audio stream: %s", SDL_GetError());
        }
        
        tick_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr, nullptr, nullptr);
        if (tick_stream) {
            SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(tick_stream));
            SDL_SetAudioStreamGain(tick_stream, master_volume);
        }

        LoadWAV("Assets/Sound/Move.wav", snd_move);
        LoadWAV("Assets/Sound/Capture.wav", snd_capture);
        LoadWAV("Assets/Sound/Complete.wav", snd_correct); 
        LoadWAV("Assets/Sound/Defeat.wav", snd_wrong);     
        LoadWAV("Assets/Sound/Check.wav", snd_check);
        LoadWAV("Assets/Sound/Checkmate.wav", snd_checkmate);
        
        LoadWAV("Assets/Sound/Select.wav", snd_select);
        LoadWAV("Assets/Sound/Confirmation.wav", snd_confirm); 
        LoadWAV("Assets/Sound/Startup.wav", snd_startup);
        LoadWAV("Assets/Sound/Tick.wav", snd_tick);
        LoadWAV("Assets/Sound/Shatter.wav", snd_shatter);
    }
    
    void Init(SDL_Window* window) {
        theme_manager.ScanAssets();
        theme_manager.LoadPieceSet(theme_manager.current_piece_set);

        SDL_Surface* icon = IMG_Load("Assets/Piece/gioco/wN.svg");
        if (!icon) icon = IMG_Load("../../Assets/Piece/gioco/wN.svg");
        if (icon) {
            SDL_SetWindowIcon(window, icon);
            SDL_DestroySurface(icon);
        }

        InitAudio();
    }
    
    void Cleanup() {
        if (audio_stream) SDL_DestroyAudioStream(audio_stream);
        if (tick_stream) SDL_DestroyAudioStream(tick_stream);
        if (snd_move.buffer) SDL_free(snd_move.buffer);
        if (snd_capture.buffer) SDL_free(snd_capture.buffer);
        if (snd_correct.buffer) SDL_free(snd_correct.buffer);
        if (snd_wrong.buffer) SDL_free(snd_wrong.buffer);
        if (snd_check.buffer) SDL_free(snd_check.buffer);
        if (snd_checkmate.buffer) SDL_free(snd_checkmate.buffer);
        if (snd_select.buffer) SDL_free(snd_select.buffer);
        if (snd_confirm.buffer) SDL_free(snd_confirm.buffer);
        if (snd_startup.buffer) SDL_free(snd_startup.buffer);
        if (snd_tick.buffer) SDL_free(snd_tick.buffer);
        if (snd_shatter.buffer) SDL_free(snd_shatter.buffer);

        for (int i = 0; i < 16; ++i) {
            if (theme_manager.piece_textures[i] != 0) glDeleteTextures(1, &theme_manager.piece_textures[i]);
        }
        if (theme_manager.board_texture) glDeleteTextures(1, &theme_manager.board_texture);
        if (theme_manager.bg_texture) glDeleteTextures(1, &theme_manager.bg_texture);
    }

    // --- New Feature Stubs ---
    void LoadSoundTheme(const std::string& theme_name) {
        // TODO: Implement sound theme loading
    }
    
    SoundEffect* GetSound(const std::string& name) {
        // TODO: Return sound by string name
        return nullptr; 
    }
};
