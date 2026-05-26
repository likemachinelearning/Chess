#pragma once
#include <string>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include "GameContext.hpp"
#include "AssetManager.hpp"

// Pure Data Pod for serialization
struct AppSettingsState {
    bool show_controls = true;
    bool show_history = true;
    bool show_move_list = false;
    bool show_stats = false;
    bool show_engine_tab = false;
    bool show_eval_chart = false;
    bool show_timer = true;
    bool show_side_to_move = true;
    bool show_dashboard_engine_stats = true;
    bool show_system_usage = false;
    bool show_batch_dashboard = false;
    bool show_ai_coach = false;
    bool show_eval_bar_sf = true;
    bool show_eval_bar_lc0 = false;
    bool show_coordinates = true;
    bool show_performance_analytics = false;
    int current_app_theme = 0;
    float combo_text_timeout = 30.0f;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(AppSettingsState, 
        show_controls, show_history, show_move_list, show_stats, 
        show_engine_tab, show_eval_chart, show_timer, show_side_to_move, 
        show_dashboard_engine_stats, show_system_usage, show_batch_dashboard, show_ai_coach,
        show_eval_bar_sf, show_eval_bar_lc0, show_coordinates, show_performance_analytics, 
        current_app_theme, combo_text_timeout
    )
};

class SettingsManager {
public:
    AppSettingsState state;

    void Save(const std::string& path, GameContext* context, AssetManager* assets) {
        nlohmann::json j;

        j["ui"] = state;
        j["effects"] = context->game.effects;

        // Engine Settings
        j["sf_threads"] = context->game.sf_engine.threads;
        j["sf_hash"] = context->game.sf_engine.hash;
        j["sf_difficulty"] = context->game.sf_engine.skill_level;
        j["sf_move_overhead"] = context->game.sf_engine.move_overhead;
        j["sf_show_wdl"] = context->game.sf_engine.show_wdl;
        j["sf_ponder"] = context->game.sf_engine.ponder;
        j["sf_depth"] = context->game.sf_engine.limit_depth;
        j["sf_multipv"] = context->game.sf_engine.multipv;

        j["lc0_threads"] = context->game.lc0_engine.threads;
        j["lc0_nodes"] = context->game.lc0_engine.limit_nodes;
        j["lc0_batch_size"] = context->game.lc0_engine.batch_size;
        j["lc0_backend"] = context->game.lc0_engine.backend;
        j["lc0_weights"] = context->game.lc0_engine.weights_path;
        j["lc0_nncache"] = context->game.lc0_engine.nn_cache_size;
        j["lc0_minibatch"] = context->game.lc0_engine.minibatch_size;
        j["lc0_multipv"] = context->game.lc0_engine.multipv;
        j["lc0_ponder"] = context->game.lc0_engine.ponder;
        j["lc0_show_wdl"] = context->game.lc0_engine.show_wdl;
        
        // Other Settings
        j["is_flipped"] = context->game.is_flipped;
        j["auto_next"] = context->game.auto_next;
        j["sf_active"] = context->game.sf_active;
        j["lc0_active"] = context->game.lc0_active;
        j["pov_player_name"] = context->pov_player_name;
        j["master_volume"] = assets->master_volume;
        j["theme_idx"] = assets->theme_manager.current_theme_idx;
        j["piece_theme_idx"] = assets->theme_manager.current_piece_theme_idx;
        j["board_theme_idx"] = assets->theme_manager.current_board_theme_idx;
        j["font_scale"] = assets->theme_manager.global_font_scale;
        j["enable_sound"] = assets->enable_sound;

        std::ofstream o(path);
        if (o.is_open()) o << std::setw(4) << j << std::endl;
    }

    void Load(const std::string& path, GameContext* context, AssetManager* assets) {
        std::ifstream i(path);
        if (!i.is_open()) return;

        nlohmann::json j;
        try { i >> j; } catch (...) { return; }

        try {
            if (j.contains("ui")) {
                try {
                    state = j["ui"].get<AppSettingsState>();
                } catch (...) {
                    // If loading fails (e.g. missing keys due to update), keep defaults
                }
            }

            if (j.contains("effects")) {
                try {
                    j["effects"].get_to(context->game.effects);
                } catch (...) {
                    // If loading fails (e.g. old schema or corrupted data), keep defaults
                }
            }

            if(j.contains("sf_threads")) context->game.sf_engine.threads = j["sf_threads"];
            if(j.contains("sf_hash")) context->game.sf_engine.hash = j["sf_hash"];
            if(j.contains("sf_difficulty")) context->game.sf_engine.skill_level = j["sf_difficulty"];
            if(j.contains("sf_move_overhead")) context->game.sf_engine.move_overhead = j["sf_move_overhead"];
            if(j.contains("sf_show_wdl")) context->game.sf_engine.show_wdl = j["sf_show_wdl"];
            if(j.contains("sf_ponder")) context->game.sf_engine.ponder = j["sf_ponder"];
            if(j.contains("sf_depth")) context->game.sf_engine.limit_depth = j["sf_depth"];
            if(j.contains("sf_multipv")) context->game.sf_engine.multipv = j["sf_multipv"];

            if(j.contains("lc0_threads")) context->game.lc0_engine.threads = j["lc0_threads"];
            if(j.contains("lc0_nodes")) context->game.lc0_engine.limit_nodes = j["lc0_nodes"];
            if(j.contains("lc0_batch_size")) context->game.lc0_engine.batch_size = j["lc0_batch_size"];
            if(j.contains("lc0_backend")) context->game.lc0_engine.backend = j["lc0_backend"];
            if(j.contains("lc0_weights")) context->game.lc0_engine.weights_path = j["lc0_weights"];
            if(j.contains("lc0_nncache")) context->game.lc0_engine.nn_cache_size = j["lc0_nncache"];
            if(j.contains("lc0_minibatch")) context->game.lc0_engine.minibatch_size = j["lc0_minibatch"];
            if(j.contains("lc0_multipv")) context->game.lc0_engine.multipv = j["lc0_multipv"];
            if(j.contains("lc0_ponder")) context->game.lc0_engine.ponder = j["lc0_ponder"];
            if(j.contains("lc0_show_wdl")) context->game.lc0_engine.show_wdl = j["lc0_show_wdl"];

            if(j.contains("is_flipped")) context->game.is_flipped = j["is_flipped"];
            if(j.contains("auto_next")) context->game.auto_next = j["auto_next"];
            if(j.contains("sf_active")) context->game.sf_active = j["sf_active"];
            if(j.contains("lc0_active")) context->game.lc0_active = j["lc0_active"];
            if(j.contains("pov_player_name")) context->pov_player_name = j["pov_player_name"];
            if(j.contains("master_volume")) assets->master_volume = j["master_volume"];
            if(j.contains("enable_sound")) assets->enable_sound = j["enable_sound"];
            
            if(j.contains("theme_idx")) assets->theme_manager.current_theme_idx = j["theme_idx"];
            if(j.contains("piece_theme_idx")) assets->theme_manager.current_piece_theme_idx = j["piece_theme_idx"];
            if(j.contains("board_theme_idx")) assets->theme_manager.current_board_theme_idx = j["board_theme_idx"];
            if(j.contains("font_scale")) assets->theme_manager.global_font_scale = j["font_scale"];
        } catch (...) {
            // Overall fallback for any type mismatches in engine settings
        }
    }
};