#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <regex>
#include <thread>
#include <atomic>
#include <mutex>
#include <future>

#include <SDL3/SDL.h>
#include <imgui.h> // For ImVec4, ImVec2
#include <nlohmann/json.hpp>

#include "chess.hpp"
#include "PuzzleManager.hpp"
#include "EngineManager.hpp"
#include "EffectsManager.hpp"
#include "AssetManager.hpp"
#include "StatsManager.hpp"

// Windows specific for SystemMonitor and WinHTTP
#ifdef _WIN32
#include <pdh.h>
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

using namespace chess;

// --- Structs ---

struct ParsedGame {
    std::string start_fen = constants::STARTPOS;
    std::vector<Move> move_history;
    std::vector<std::string> san_history;
    std::string white_player = "White";
    std::string black_player = "Black";
    std::string result = "*";
    std::string date = "?";
    std::string event = "Imported Game";
};

struct SystemMonitor {
    #ifdef _WIN32
    PDH_HQUERY cpuQuery = NULL;
    PDH_HCOUNTER cpuTotal = NULL;
    PDH_HQUERY gpuQuery = NULL;
    PDH_HCOUNTER gpuTotal = NULL;
    #endif
    
    double cpuUsage = 0.0;
    std::map<int, double> gpuUsages;
    bool initialized = false;
    float last_sys_update = 0.0f;

    void Init() {
        if (initialized) return;
        #ifdef _WIN32
        if (PdhOpenQueryA(NULL, 0, &cpuQuery) == ERROR_SUCCESS) {
            PdhAddEnglishCounterA(cpuQuery, "\\Processor(_Total)\\% Processor Time", 0, &cpuTotal);
            PdhCollectQueryData(cpuQuery);
        }

        if (PdhOpenQueryA(NULL, 0, &gpuQuery) == ERROR_SUCCESS) {
            if (PdhAddEnglishCounterA(gpuQuery, "\\GPU Engine(*)\\Utilization Percentage", 0, &gpuTotal) != ERROR_SUCCESS) {
                 // Fallback
            }
            PdhCollectQueryData(gpuQuery);
        }
        #endif
        initialized = true;
    }

    void Update(float dt) {
        if (!initialized) return;

        last_sys_update += dt;
        if (last_sys_update < 1.0f) return;
        last_sys_update = 0.0f;

        #ifdef _WIN32
        if (cpuQuery) {
            PdhCollectQueryData(cpuQuery);
            PDH_FMT_COUNTERVALUE counterVal;
            if (PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal) == ERROR_SUCCESS) {
                cpuUsage = counterVal.doubleValue;
            }
        }        
        if (gpuQuery && gpuTotal) {
            PdhCollectQueryData(gpuQuery);
            
            DWORD bufferSize = 0;
            DWORD itemCount = 0;
            PdhGetFormattedCounterArrayA(gpuTotal, PDH_FMT_DOUBLE, &bufferSize, &itemCount, NULL);
            
            if (bufferSize > 0) {
                std::vector<char> buffer(bufferSize);
                if (PdhGetFormattedCounterArrayA(gpuTotal, PDH_FMT_DOUBLE, &bufferSize, &itemCount, (PPDH_FMT_COUNTERVALUE_ITEM_A)buffer.data()) == ERROR_SUCCESS) {
                    
                    gpuUsages.clear();
                    PPDH_FMT_COUNTERVALUE_ITEM_A items = (PPDH_FMT_COUNTERVALUE_ITEM_A)buffer.data();
                    
                    for (DWORD i = 0; i < itemCount; ++i) {
                        double val = items[i].FmtValue.doubleValue;
                        if (items[i].szName) {
                            std::string name = items[i].szName;
                            size_t pos = name.find("phys_");
                            if (pos != std::string::npos && pos + 5 < name.length()) {
                                int id = std::atoi(name.c_str() + pos + 5);
                                if (val > gpuUsages[id]) {
                                    gpuUsages[id] = val;
                                }
                            }
                        }
                    }
                }
            }
        }
        #endif
    }
};

struct DragState {
    bool is_dragging = false;
    Square source_sq = Square::underlying::NO_SQ;
    ImVec2 mouse_offset = ImVec2(0, 0); 
    int piece_idx = -1; 
};

struct PromotionState {
    bool active = false;
    Square from = Square::underlying::NO_SQ;
    Square to = Square::underlying::NO_SQ;
    Color side = Color::WHITE;
};

struct CalendarState {
    int year = 2026;
    int month = 1; 
    
    CalendarState() {
        std::time_t t = std::time(nullptr);
        std::tm now_tm;
        #if defined(_WIN32) || defined(_WIN64)
            localtime_s(&now_tm, &t);
        #else
            localtime_r(&t, &now_tm);
        #endif
        year = now_tm.tm_year + 1900;
        month = now_tm.tm_mon + 1;
    }
};

struct Arrow {
    Square from;
    Square to;
    bool operator==(const Arrow& other) const {
        return from == other.from && to == other.to;
    }
};

struct MoveAnimation {
    bool active = false;
    Square from = Square::underlying::NO_SQ;
    Square to = Square::underlying::NO_SQ;
    Uint64 start_time = 0;
    Uint64 duration = 350; 
    int piece_idx = -1;
    bool is_correct_puzzle_move = false;
};

struct MoveNode {
    chess::Move move = chess::Move::NO_MOVE;
    std::string san;
    float eval = 0.0f;
    std::string eval_str = ""; // Saved from deep analysis
    
    std::string live_sf_eval = ""; // Saved dynamically as user browses
    std::string live_lc0_eval = "";

    int parent_idx = -1;
    std::vector<int> children; // Indices of variation branches
};

struct GameState {
    std::string start_fen = constants::STARTPOS;
    
    std::vector<MoveNode> move_tree;
    int current_node_idx = -1; // -1 means at start_fen

    Board board; 
    bool is_flipped = false;
    float flip_val = 0.0f; // 0.0 to 1.0
    bool auto_next = false;
    float auto_next_delay = 1.0f;
    float auto_next_timer = -1.0f; 
    double puzzle_timer = 0.0;
    float puzzle_tti = 0.0f; // Time-to-Insight
    bool tti_captured = false;
    bool timer_running = false;
    bool review_mode = false;
    bool focus_mode = false;
    int hint_level = 0; 
    
    // Engine & Effects
    EngineManager sf_engine{"Stockfish"};
    EngineManager lc0_engine{"Lc0"};
    
    // Dedicated Auto Play Engines (Isolated Memory/TT)
    EngineManager sf_engine_white{"Stockfish_W"};
    EngineManager sf_engine_black{"Stockfish_B"};
    EngineManager lc0_engine_white{"Lc0_W"};
    EngineManager lc0_engine_black{"Lc0_B"};
    
    int user_sf_depth = 21;
    int user_lc0_nodes = 10000;
    bool sf_active = false;
    bool lc0_active = false;
    bool suppress_engine_update = false;
    
    EffectsManager effects;
    
    // Arrows / Drawing
    std::vector<Arrow> arrows;
    Arrow current_drawing_arrow = { Square::underlying::NO_SQ, Square::underlying::NO_SQ };
    bool is_right_dragging = false;
    
    // Preview Mode
    bool preview_mode = false;
    bool preview_animating = false;
    int preview_anim_idx = 0;
    std::vector<Move> preview_moves;

    // Stats for display after solve
    double last_solve_time = 0.0;
    double last_total_time = 0.0;
    bool was_solved = false;
    bool forgiven_mistake_used = false;
    int shield_recharge_target = 0;

    enum class EvalChartStyle { BASIC, DUAL, NEON, HYBRID };    EvalChartStyle chart_style = EvalChartStyle::HYBRID;
    bool board_dirty = true;
    int combo_count = 0;
    int max_combo = 0;

    bool is_in_check = false;
    chess::Square checked_king_sq = chess::Square::underlying::NO_SQ;
    std::vector<chess::Square> checking_piece_squares;

    // Auto Play
    enum class AutoPlayPlayer { HUMAN = 0, STOCKFISH = 1, LC0 = 2 };
    bool auto_play_active = false;
    AutoPlayPlayer auto_play_white = AutoPlayPlayer::STOCKFISH;
    bool auto_play_white_use_time = true;
    float auto_play_white_move_time = 1.0f;
    int auto_play_white_depth = 15;
    int auto_play_white_nodes = 800;
    AutoPlayPlayer auto_play_black = AutoPlayPlayer::STOCKFISH;
    bool auto_play_black_use_time = true;
    float auto_play_black_move_time = 1.0f;
    int auto_play_black_depth = 15;
    int auto_play_black_nodes = 800;
    float auto_play_timer = 0.0f;
    bool auto_play_restart = false;
    bool auto_play_save_pgn = false;
    bool auto_play_use_separate_instances = true;
    int auto_play_opening_idx = 1; // 0 = Random, 1+ = Specific Opening (1 = Standard)
    std::string auto_play_last_search_fen = "";
    float auto_play_end_timer = 0.0f;
    int auto_play_session_game_count = 0;
    int ap_wins_white = 0;
    int ap_wins_black = 0;
    int ap_draws = 0;

    // Game Over Banner
    bool banner_active = false;
    std::string banner_text = "";
    ImVec4 banner_color = ImVec4(1, 1, 1, 1);
    float banner_timer = 0.0f;
    bool game_over_recorded = false;
    
    // Opening Notification
    bool opening_notif_active = false;
    std::string opening_notif_text = "";
    float opening_notif_timer = 0.0f;

    struct OpeningDef { std::string name; std::string fen; };
    const std::vector<OpeningDef>& GetAvailableOpenings() const {
        static const std::vector<OpeningDef> openings = {
            {"Standard Starting Position", chess::constants::STARTPOS},
            {"Sicilian Defense", "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2"},
            {"French Defense", "rnbqkbnr/pppp1ppp/4p3/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2"},
            {"Ruy Lopez", "r1bqkbnr/pppp1ppp/2n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3"},
            {"Queen's Gambit", "rnbqkbnr/ppp1pppp/8/3p4/2PP4/8/PP2PPPP/RNBQKBNR b KQkq c3 0 2"},
            {"Caro-Kann", "rnbqkbnr/pp1ppppp/2p5/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2"},
            {"English Opening", "rnbqkbnr/pppppppp/8/8/2P5/8/PP1PPPPP/RNBQKBNR b KQkq c3 0 1"},
            {"Indian Game", "rnbqkb1r/pppppppp/5n2/8/3P4/8/PPP1PPPP/RNBQKBNR w KQkq - 1 2"},
            {"Two Knights Defense", "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4"}
        };
        return openings;
    }

    std::pair<std::string, std::string> GetSelectedOpening() const {
        const auto& openings = GetAvailableOpenings();
        if (auto_play_opening_idx == 0) {
            int r = rand() % openings.size();
            return {openings[r].name, openings[r].fen};
        }
        int idx = auto_play_opening_idx - 1;
        if (idx >= 0 && idx < openings.size()) return {openings[idx].name, openings[idx].fen};
        return {openings[0].name, openings[0].fen};
    }

    std::string GetPlayerStr(AutoPlayPlayer player, int depth, int nodes) const {
        if (player == AutoPlayPlayer::HUMAN) return "Human";
        if (player == AutoPlayPlayer::STOCKFISH) return "SF_D" + std::to_string(depth);
        if (player == AutoPlayPlayer::LC0) return "Lc0_N" + std::to_string(nodes);
        return "Unknown";
    }

    void SaveAutoplayPGN(std::string winner_str) {
        std::string dir = "Assets/user_data/Autoplay";
        if (!std::filesystem::exists(dir)) std::filesystem::create_directories(dir);

        auto_play_session_game_count++;
        
        std::time_t t = std::time(nullptr);
        std::tm tm;
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        char date_buf[64];
        std::strftime(date_buf, sizeof(date_buf), "%Y%m%d", &tm);
        
        char count_buf[16];
        snprintf(count_buf, sizeof(count_buf), "%04d", auto_play_session_game_count);

        std::string w_player = GetPlayerStr(auto_play_white, auto_play_white_depth, auto_play_white_nodes);
        std::string b_player = GetPlayerStr(auto_play_black, auto_play_black_depth, auto_play_black_nodes);
        
        std::string filename = std::string(count_buf) + "_" + date_buf + "_" + w_player + "_vs_" + b_player + "_" + winner_str + ".pgn";
        
        std::ofstream out(dir + "/" + filename);
        if (out.is_open()) {
            out << "[Event \"Auto Play Match\"]\n";
            out << "[Date \"" << date_buf << "\"]\n";
            out << "[White \"" << w_player << "\"]\n";
            out << "[Black \"" << b_player << "\"]\n";
            out << "[FEN \"" << start_fen << "\"]\n";
            out << "\n" << GeneratePGN() << "\n\n";
            out.close();
        }
    }

    std::string GeneratePGN() const {
        std::string pgn = "";
        std::vector<int> main_branch = get_main_branch_indices();
        
        for (int i = 0; i < main_branch.size(); ++i) {
            int node_idx = main_branch[i];
            const MoveNode& node = move_tree[node_idx];
            
            if (i % 2 == 0) {
                pgn += std::to_string((i / 2) + 1) + ". ";
            }
            pgn += node.san + " ";
        }
        
        auto game_result = board.isGameOver().second;
        if (game_result == chess::GameResult::LOSE) {
            if (board.sideToMove() == Color::WHITE) pgn += "0-1";
            else pgn += "1-0";
        } else if (game_result == chess::GameResult::DRAW) {
            pgn += "1/2-1/2";
        } else {
            pgn += "*";
        }
        
        return pgn;
    }

    void UpdateEngines(bool force = false) {
        if (suppress_engine_update) return;
        if (!board_dirty && !force) return;

        std::string fen = board.getFen();
        
        auto FindAsset = [](std::string path) -> std::string {
            if (std::filesystem::exists(path)) return std::filesystem::absolute(path).string();
            if (std::filesystem::exists("../" + path)) return std::filesystem::absolute("../" + path).string();
            if (std::filesystem::exists("../../" + path)) return std::filesystem::absolute("../../" + path).string();
            return "";
        };

        // Apply dynamic limits based on Auto Play settings
        sf_engine.limit_depth = user_sf_depth;
        lc0_engine.limit_nodes = user_lc0_nodes;
        if (auto_play_active && board.isGameOver().second == chess::GameResult::NONE) {
            Color side = board.sideToMove();
            AutoPlayPlayer current_player = (side == Color::WHITE) ? auto_play_white : auto_play_black;
            if (current_player == AutoPlayPlayer::STOCKFISH) {
                sf_engine.limit_depth = (side == Color::WHITE) ? auto_play_white_depth : auto_play_black_depth;
                sf_engine.ClearHash(); // Prevent TT sharing between White and Black
            } else if (current_player == AutoPlayPlayer::LC0) {
                lc0_engine.limit_nodes = (side == Color::WHITE) ? auto_play_white_nodes : auto_play_black_nodes;
            }
        }

        if (sf_active) {
            if (!sf_engine.IsRunning()) {
                std::string path = FindAsset("Assets/Engines/stockfish/stockfish-windows-x86-64-avx512icl.exe");
                if (path.empty()) path = "stockfish"; // Fallback to PATH
                sf_engine.StartEngine("\"" + path + "\"");
            }
            // Always update position if active
            sf_engine.StartAnalysis(fen);
        } else {
            sf_engine.StopAnalysis();
        }
        
        if (lc0_active) {
            if (!lc0_engine.IsRunning()) {
                std::string exe_path = FindAsset("Assets/Engines/lc0/lc0.exe");
                std::string w_path = lc0_engine.weights_path;
                
                if (w_path.empty()) {
                    w_path = FindAsset("Assets/Engines/lc0/791556.pb.gz");
                } else if (std::filesystem::exists(w_path)) {
                    w_path = std::filesystem::absolute(w_path).string();
                } else {
                     // Try finding the user configured weight relative to assets
                     std::string found = FindAsset(w_path);
                     if (!found.empty()) w_path = found;
                }
                
                std::string work_dir = "";
                if (!exe_path.empty()) work_dir = std::filesystem::path(exe_path).parent_path().string();
                
                if (!exe_path.empty() && !w_path.empty()) {
                    std::string cmd = "\"" + exe_path + "\" --weights=\"" + w_path + "\" --backend=" + lc0_engine.backend;
                    lc0_engine.StartEngine(cmd, work_dir);
                } else {
                    std::cerr << "[Lc0] Critical Error: Engine or Weights file not found." << std::endl;
                    lc0_active = false;
                }
            }
            
            // If running (started successfully), update analysis
            if (lc0_engine.IsRunning()) {
                lc0_engine.StartAnalysis(fen);
            }
        } else {
            lc0_engine.StopAnalysis();
        }
        
        board_dirty = false;
    }

    void reset(std::string fen = constants::STARTPOS) {
        start_fen = fen;
        move_tree.clear();
        current_node_idx = -1;
        board = Board(fen);
        is_flipped = false;
        auto_next_timer = -1.0f;
        timer_running = false;
        puzzle_timer = 0.0;
        was_solved = false;
        hint_level = 0;
        arrows.clear();
        is_right_dragging = false;
        board_dirty = true;
        
        banner_active = false;
        banner_timer = 0.0f;
        game_over_recorded = false;
        
        opening_notif_active = true;
        opening_notif_timer = 3.0f;
        opening_notif_text = "Standard Starting Position";
        for (const auto& op : GetAvailableOpenings()) {
            if (op.fen == fen) {
                opening_notif_text = "Opening: " + op.name;
                break;
            }
        }
        
        UpdateEngines();
    }
    
    void make_move(Move m) {
        // Generate SAN before making the move on the board
        std::string san = uci::moveToSan(board, m);
        
        int new_node_idx = -1;
        if (current_node_idx == -1) {
            // Check if root nodes have this move
            for (int i = 0; i < move_tree.size(); ++i) {
                if (move_tree[i].parent_idx == -1 && move_tree[i].move == m) {
                    new_node_idx = i;
                    break;
                }
            }
        } else {
            // Check children of current node
            for (int child_idx : move_tree[current_node_idx].children) {
                if (move_tree[child_idx].move == m) {
                    new_node_idx = child_idx;
                    break;
                }
            }
        }
        
        if (new_node_idx == -1) {
            // Create new node
            MoveNode node;
            node.move = m;
            node.san = san;
            node.parent_idx = current_node_idx;
            move_tree.push_back(node);
            new_node_idx = (int)move_tree.size() - 1;
            
            if (current_node_idx != -1) {
                move_tree[current_node_idx].children.push_back(new_node_idx);
            }
        }
        
        current_node_idx = new_node_idx;
        board.makeMove(m);
        arrows.clear(); 
        is_right_dragging = false;
        board_dirty = true;
        
        UpdateEngines();
    }

    std::vector<int> get_current_line_indices() const {
        std::vector<int> line;
        int curr = current_node_idx;
        while (curr != -1 && curr < move_tree.size()) {
            line.push_back(curr);
            curr = move_tree[curr].parent_idx;
        }
        std::reverse(line.begin(), line.end());
        return line;
    }

    std::vector<int> get_active_branch_indices() const {
        std::vector<int> line = get_current_line_indices();
        int curr = current_node_idx;
        
        if (curr == -1) {
            // Find root
            for (int i = 0; i < move_tree.size(); ++i) {
                if (move_tree[i].parent_idx == -1) {
                    curr = i;
                    line.push_back(curr);
                    break;
                }
            }
        }

        while (curr != -1 && curr < move_tree.size() && !move_tree[curr].children.empty()) {
            // Follow the last added child (most recent variation) or child 0? 
            // Usually we want to follow child 0 as the main continuation of this branch.
            curr = move_tree[curr].children[0];
            line.push_back(curr);
        }
        return line;
    }

    void rebuild_board() {
        board = Board(start_fen);
        auto line = get_current_line_indices();
        for (int idx : line) {
            board.makeMove(move_tree[idx].move);
        }
        board_dirty = true;
        UpdateEngines();
    }

    void go_back() {
        if (current_node_idx != -1) {
            current_node_idx = move_tree[current_node_idx].parent_idx;
            rebuild_board();
        }
    }
    
    void go_forward(int child_index = 0) {
        if (current_node_idx == -1) {
            for (int i = 0; i < move_tree.size(); ++i) {
                if (move_tree[i].parent_idx == -1) {
                    current_node_idx = i;
                    rebuild_board();
                    return;
                }
            }
        } else {
            if (!move_tree[current_node_idx].children.empty() && child_index < move_tree[current_node_idx].children.size()) {
                current_node_idx = move_tree[current_node_idx].children[child_index];
                rebuild_board();
            }
        }
    }
    
    void jump_to_move(int m) {
        auto main_branch = get_main_branch_indices();
        if (m <= 0) {
            jump_to_node(-1);
        } else if (m <= (int)main_branch.size()) {
            jump_to_node(main_branch[m - 1]);
        }
    }

    void jump_to_node(int node_idx) {
        if (node_idx >= -1 && node_idx < (int)move_tree.size()) {
            current_node_idx = node_idx;
            rebuild_board();
        }
    }
    std::vector<int> get_main_branch_indices() const {
        std::vector<int> indices;
        int curr = -1;
        
        if (move_tree.empty()) return indices;
        
        // Find first root node
        for (int i = 0; i < move_tree.size(); ++i) {
            if (move_tree[i].parent_idx == -1) {
                curr = i;
                break;
            }
        }
        
        while (curr != -1) {
            indices.push_back(curr);
            if (!move_tree[curr].children.empty()) {
                curr = move_tree[curr].children[0];
            } else {
                curr = -1;
            }
        }
        return indices;
    }
};

struct GameInsight {
    enum class Type { BLUNDER, MISSED_FORK, HANGING_PIECE, BACK_RANK, OPENING_EFFICIENCY };
    Type type;
    int game_idx;
    int move_idx;
    float eval_before;
    float eval_after;
    std::string description;
};

struct GameSummary {
    int game_idx;
    std::string white, black;
    std::string white_name_cached, black_name_cached; // Cache for UI
    float accuracy = 0.0f;
    float acpl = 0.0f; // Average Centipawn Loss
    int blunders = 0;
    int mistakes = 0;
    int inaccuracies = 0;
    int greats = 0;
    int brilliants = 0;
    
    // Advanced Metrics
    float fighting_spirit = 0.0f; // Accuracy in losing positions
    float punishment = 0.0f;      // Efficiency after opponent blunder
    
    // Phase Accuracies
    float opAcc = 0.0f, midAcc = 0.0f, endAcc = 0.0f;

    bool deserves_deep_dive = false;
    std::vector<int> move_qualities;
    std::vector<float> move_evals;
};

struct BatchReport {
    int total_games = 0;
    int games_scouted = 0;
    int deep_dives_completed = 0;
    std::vector<GameSummary> summaries;
    std::vector<GameInsight> insights;
    std::vector<std::string> feed_log;
    
    int total_blunders = 0;
    int total_brilliants = 0;
    float avg_batch_accuracy = 0.0f;
};

struct BatchAnalysisState {
    bool active = false;
    int pass = 1; 
    int current_game_idx = 0;
    int current_move_idx = 0;
    bool waiting_for_engine = false;
    float last_eval = 0.0f; 
    BatchReport report;

    int scout_depth = 12;
    bool enable_deep_dive = true;

    // Helpers for Accuracy
    float toWinPercent(float cp) {
        float capped = std::clamp(cp, -6.0f, 6.0f) * 100.0f;
        return 50.0f + 50.0f * (2.0f / 3.14159265f) * atanf(capped / 290.6806f);
    }
    float calcAccuracy(float winLoss) {
        float acc = 103.1668f * expf(-0.08f * winLoss) - 3.1669f;
        return std::clamp(acc, 0.0f, 100.0f);
    }
    int getMaterial(const Board& b) {
        int score = 0;
        for (int sq = 0; sq < 64; ++sq) {
            Piece p = b.at<Piece>(Square(sq));
            if (p == Piece::NONE) continue;
            PieceType pt = p.type();
            if (pt == PieceType::QUEEN) score += 9;
            else if (pt == PieceType::ROOK) score += 5;
            else if (pt == PieceType::BISHOP || pt == PieceType::KNIGHT) score += 3;
            else if (pt == PieceType::PAWN) score += 1;
        }
        return score;
    }
};

struct LLMCoach {
    std::string endpoint_url = "http://localhost:11434/api/generate";
    std::string model_name = "llama3";
    std::string response_text = "";
    std::string metrics_text = "";
    char user_input_buf[1024] = "";
    bool show_config = false;
    std::atomic<bool> is_thinking = false;

    LLMCoach() {
        endpoint_url.resize(256);
        model_name.resize(128);
    }

    void RequestAnalysis(std::string fen, std::string sf_eval, std::string lc0_eval, std::string user_question = "") {
        if (is_thinking) return;
        is_thinking = true;
        response_text = "";
        metrics_text = "";

        std::thread([this, fen, sf_eval, lc0_eval, user_question]() {
            std::string prompt = "You are an expert chess coach. The current position is " + fen + ". Stockfish evaluates this as " + sf_eval + " and Lc0 evaluates this as " + lc0_eval + ". ";
            
            if (!user_question.empty()) {
                prompt += "The player asks: \"" + user_question + "\". Answer their question directly and concisely based on the engine evaluations and position.";
            } else {
                prompt += "Explain to a club-level player why these moves are suggested and what the strategic plans are for both sides. Be concise.";
            }

            nlohmann::json j_payload;
            
            bool is_chat = (endpoint_url.find("chat") != std::string::npos || endpoint_url.find("v1/chat") != std::string::npos);
            if (is_chat) {
                j_payload["model"] = model_name;
                j_payload["messages"] = nlohmann::json::array();
                nlohmann::json msg;
                msg["role"] = "user";
                msg["content"] = prompt;
                j_payload["messages"].push_back(msg);
                j_payload["temperature"] = 0.7;
            } else {
                j_payload["model"] = model_name;
                j_payload["prompt"] = prompt;
                j_payload["stream"] = false;
            }

            std::string payload = j_payload.dump();

            #ifdef _WIN32
            URL_COMPONENTS urlComp;
            ZeroMemory(&urlComp, sizeof(urlComp));
            urlComp.dwStructSize = sizeof(urlComp);
            
            std::wstring w_url = std::wstring(endpoint_url.begin(), endpoint_url.end());
            wchar_t hostName[256];
            wchar_t urlPath[1024];
            urlComp.lpszHostName = hostName;
            urlComp.dwHostNameLength = 256;
            urlComp.lpszUrlPath = urlPath;
            urlComp.dwUrlPathLength = 1024;

            if (WinHttpCrackUrl(w_url.c_str(), 0, 0, &urlComp)) {
                HINTERNET hSession = WinHttpOpen(L"LLMCoach/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
                if (hSession) {
                    HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
                    if (hConnect) {
                        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", urlPath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
                        if (hRequest) {
                            std::wstring headers = L"Content-Type: application/json\r\n";
                            if (WinHttpSendRequest(hRequest, headers.c_str(), -1, (LPVOID)payload.c_str(), (DWORD)payload.length(), (DWORD)payload.length(), 0)) {
                                if (WinHttpReceiveResponse(hRequest, NULL)) {
                                    std::string response_data;
                                    DWORD dwSize = 0;
                                    DWORD dwDownloaded = 0;
                                    do {
                                        dwSize = 0;
                                        if (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0) {
                                            char* pszOutBuffer = new char[dwSize + 1];
                                            if (WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded)) {
                                                pszOutBuffer[dwDownloaded] = '\0';
                                                response_data += pszOutBuffer;
                                            }
                                            delete[] pszOutBuffer;
                                        }
                                    } while (dwSize > 0);

                                    try {
                                        auto j_res = nlohmann::json::parse(response_data);
                                        if (j_res.contains("response")) {
                                            response_text = j_res["response"].get<std::string>();
                                        } else if (j_res.contains("choices") && j_res["choices"].is_array() && !j_res["choices"].empty() && j_res["choices"][0].contains("message")) {
                                            response_text = j_res["choices"][0]["message"]["content"].get<std::string>();
                                        } else {
                                            response_text = "Unexpected response format.\n\n" + response_data;
                                        }

                                        if (j_res.contains("eval_count") && j_res.contains("eval_duration")) {
                                            long long eval_count = j_res["eval_count"].get<long long>();
                                            long long eval_duration = j_res["eval_duration"].get<long long>(); 
                                            if (eval_duration > 0) {
                                                double seconds = eval_duration / 1e9;
                                                double tps = eval_count / seconds;
                                                std::stringstream ss;
                                                ss << "Generated " << eval_count << " tokens in " << std::fixed << std::setprecision(1) << seconds << "s (" << std::fixed << std::setprecision(1) << tps << " t/s)";
                                                metrics_text = ss.str();
                                            }
                                        } else if (j_res.contains("usage")) {
                                            long long total_tokens = j_res["usage"]["total_tokens"].get<long long>();
                                            metrics_text = "Generated " + std::to_string(total_tokens) + " tokens.";
                                        }
                                    } catch (...) {
                                        response_text = "Error parsing response from LLM.\n\n" + response_data;
                                    }
                                } else {
                                    response_text = "Failed to receive response from local LLM. Is it running?";
                                }
                            } else {
                                response_text = "Failed to send request to local LLM. Is it running?";
                            }
                            WinHttpCloseHandle(hRequest);
                        }
                        WinHttpCloseHandle(hConnect);
                    }
                    WinHttpCloseHandle(hSession);
                }
            } else {
                response_text = "Invalid LLM Endpoint URL.";
            }
            #else
            response_text = "LLM Coach is only supported on Windows in this build.";
            #endif

            is_thinking = false;
        }).detach();
    }
};

class GameContext {
public:
    GameState game;
    DragState drag_state;
    PromotionState promo_state;
    CalendarState cal_state;
    MoveAnimation move_anim;
    PuzzleManager puzzle_manager;
    SystemMonitor sys_monitor;
    LLMCoach llm_coach;
    
    AssetManager* assets = nullptr;
    StatsManager stats;
    
    std::string puzzle_status = "Free Play";
    ImVec4 status_color = ImVec4(1,1,1,1);
    float ghost_timer = 0.0f;

    // Multi-Game PGN Support
    std::vector<ParsedGame> imported_games;
    int current_game_idx = -1;
    std::string pov_player_name = "";

    // Analysis State
    bool analysis_active = false;
    int analysis_idx = 0;
    bool analysis_waiting = false;

    // Batch Analysis
    BatchAnalysisState batch;

    // Auto Solve State
    bool auto_solve_active = false;
    enum class AutoSolveSpeed { SLOW, NORMAL, FAST, CUSTOM };
    AutoSolveSpeed auto_solve_speed = AutoSolveSpeed::NORMAL;
    float auto_solve_custom_delay = 5.0f;
    bool auto_solve_show_hint = false;
    bool auto_solve_update_db = false;
    float auto_solve_timer = 0.0f;
    enum class AutoSolveState { WAITING, HINTING, PLAYER_MOVING, WAITING_OPPONENT, OPPONENT_MOVING };
    AutoSolveState auto_solve_state = AutoSolveState::WAITING;

    // Blindfold Mode State
    bool blindfold_active = false;
    bool blindfold_use_timer = false;
    float blindfold_delay = 5.0f;
    float current_blindfold_timer = 0.0f;

    // Play Modes
    enum class PlayMode { FREE_PLAY, REVIEW, STORM, STREAK };
    PlayMode current_mode = PlayMode::FREE_PLAY;
    float storm_timer = 0.0f;
    int storm_score = 0;
    int streak_score = 0;
    int streak_lives = 3;
    int session_max_combo = 0;
    bool mode_game_over = false;
    bool session_pb_broken = false;
    float tick_timer = 0.0f;
    
    // Play vs Engine
    bool play_vs_engine = false;
    float engine_move_timer = 0.0f;

    // Keyboard Input Mode
    bool enable_keyboard_input = false;

    float idle_activity_timer = 0.0f;
    const float IDLE_THRESHOLD = 5.0f; // 5 seconds of no movement = ultra idle

    bool NeedsContinuousUpdate() const {
        // Starfield and central glow don't force continuous update if we've been idle for a while
        bool background_ambient = game.effects.enable_starfield;
        bool should_idle = background_ambient && (idle_activity_timer > game.effects.idle_delay_seconds);

        if (should_idle) return false; // Drop to GetIdleTimeout rate

        float target_flip = game.is_flipped ? 1.0f : 0.0f;
        bool rotating = std::abs(game.flip_val - target_flip) > 0.001f;

        return game.sf_active || game.lc0_active || batch.active || analysis_active ||
               move_anim.active || game.effects.HasActiveEffects() || game.auto_next_timer >= 0 ||
               rotating || auto_solve_active || (blindfold_active && current_blindfold_timer > 0.0f) ||
               (current_mode == PlayMode::STORM && !mode_game_over) || (background_ambient && !should_idle);
    }

    int GetIdleTimeout() const {
        if (idle_activity_timer > game.effects.idle_delay_seconds) {
            return 1000 / std::max(1, game.effects.idle_fps_cap); 
        }
        if (game.effects.enable_starfield) {
            return 33; // ~30 FPS for background ambient effects transition
        }
        return 100; // 10 FPS standard idle
    }

    float GetActivityFactor() const {
        if (idle_activity_timer <= game.effects.idle_delay_seconds) return 1.0f;
        
        // 1.0 second smooth transition down to idle speed
        float transition_time = 1.0f; 
        float t = (idle_activity_timer - game.effects.idle_delay_seconds) / transition_time;
        if (t > 1.0f) return 0.001f; // "Nearly zero" floor
        
        // Smooth sine-out ramp from 1.0 to 0.001
        float ramp = 0.5f * (1.0f + cosf(t * 3.14159f)); 
        return 0.001f + 0.999f * ramp;
    }
    void Init(AssetManager* assetMgr) {
        assets = assetMgr;
    }

    void LoadImportedGame(int idx) {
        if (idx < 0 || idx >= (int)imported_games.size()) return;
        current_game_idx = idx;
        const auto& pg = imported_games[idx];
        
        game.reset(pg.start_fen);
        game.suppress_engine_update = true;
        
        for (const auto& m : pg.move_history) {
            game.make_move(m);
        }
        
        // POV Logic: Auto-flip if player name matches
        if (!pov_player_name.empty()) {
            auto Lower = [](std::string s) {
                std::transform(s.begin(), s.end(), s.begin(), ::tolower);
                return s;
            };
            std::string pov = Lower(pov_player_name);
            if (Lower(pg.white_player).find(pov) != std::string::npos) {
                game.is_flipped = false; // White POV
            } else if (Lower(pg.black_player).find(pov) != std::string::npos) {
                game.is_flipped = true;  // Black POV
            }
        }

        game.suppress_engine_update = false;
        game.UpdateEngines(true);
        
        puzzle_status = "Game " + std::to_string(idx + 1) + ": " + pg.white_player + " vs " + pg.black_player;
    }
    
    void ExecuteMoveWithSound(Move move, bool play_sound = true) {
        bool is_capture = game.board.isCapture(move);
        
        if (is_capture) {
            game.effects.QueueEffect(EffectsManager::EffectType::SPARK, move.to(), IM_COL32(255, 150, 50, 255));
        }
        
        game.make_move(move); 
        game.effects.jiggle_timer = game.effects.JIGGLE_DURATION;
        
        if (!play_sound || !assets) return;

        float pitch = 1.0f;
        if (current_mode == PlayMode::STORM && game.combo_count > 0) {
            pitch = 1.0f + (std::min(game.combo_count, 10) * 0.05f);
        }

        bool in_check = game.board.inCheck();
        bool is_mate = (game.board.isGameOver().second == GameResult::LOSE); 

        if (is_mate) assets->PlaySound(assets->snd_checkmate, pitch);
        else if (in_check) assets->PlaySound(assets->snd_check, pitch);
        else if (is_capture) assets->PlaySound(assets->snd_capture, pitch);
        else assets->PlaySound(assets->snd_move, pitch);
    }

    Move GetLegalMove(const Board& board, Square from, Square to, bool* out_is_promo = nullptr) {
        Movelist moves;
        movegen::legalmoves<>(moves, board);
        
        if (out_is_promo) *out_is_promo = false;

        for (const auto& m : moves) {
            if (m.from() == from) {
                if (m.to() == to) {
                    if (m.typeOf() == Move::PROMOTION) {
                        if (out_is_promo) *out_is_promo = true;
                        return Move::NO_MOVE;
                    }
                    return m; 
                }
                if (m.typeOf() == Move::CASTLING) {
                    Square expected_king_dst = Square::underlying::NO_SQ;
                    if (m.to().file() == File::FILE_H) { 
                         expected_king_dst = Square::castling_king_square(true, board.sideToMove());
                    } else if (m.to().file() == File::FILE_A) { 
                         expected_king_dst = Square::castling_king_square(false, board.sideToMove());
                    }
                    if (expected_king_dst != Square::underlying::NO_SQ && to == expected_king_dst) {
                        return m;
                    }
                }
            }
        }
        return Move::NO_MOVE;
    }

    void TriggerAnimation(Move move, bool is_correct = false) {
        move_anim.active = true;
        move_anim.from = move.from();
        move_anim.to = move.to();
        move_anim.start_time = SDL_GetTicks();
        move_anim.is_correct_puzzle_move = is_correct;

        Piece p = game.board.at<Piece>(move.from());
        move_anim.piece_idx = (p != Piece::NONE) ? (int)p.internal() : -1;

        ghost_timer = 1.0f;
    }
    void StartPuzzle(Puzzle* p) {
        if (!p) {
            puzzle_status = "No Puzzles Available";
            return;
        }
        
        game.reset(p->fen);
        game.is_flipped = (game.board.sideToMove() == Color::WHITE);
        game.puzzle_timer = 0.0;
        game.puzzle_tti = 0.0f;
        game.tti_captured = false;
        game.timer_running = true;
        game.was_solved = false;
        promo_state.active = false;
        game.hint_level = 0; 
        
        // Blindfold Timer Reset
        if (blindfold_active) {
            current_blindfold_timer = blindfold_use_timer ? blindfold_delay : 0.0f;
        }

        if (game.review_mode) puzzle_status = "Review Mode";
        else puzzle_status = "Free Play";
        status_color = ImVec4(1, 1, 1, 1); 
        
        if (!p->san_moves.empty()) {
            std::string opp_san = p->san_moves[0];
            Movelist moves;
            movegen::legalmoves<>(moves, game.board);
            Move opp_move = uci::parseSan(game.board, opp_san, moves);
            
            if (opp_move == Move::NO_MOVE && (opp_san.back() == '+' || opp_san.back() == '#')) {
                 opp_move = uci::parseSan(game.board, opp_san.substr(0, opp_san.size()-1), moves);
            }

            if (opp_move != Move::NO_MOVE) {
                TriggerAnimation(opp_move);
                ExecuteMoveWithSound(opp_move, true);

                puzzle_manager.current_move_idx = 1;
                puzzle_status = "Your Turn (" + std::string(game.board.sideToMove() == Color::WHITE ? "White" : "Black") + ")";
                status_color = ImVec4(1, 1, 1, 1);
            } else {
                puzzle_status = "Error: Start move failed (" + opp_san + ")";
                status_color = ImVec4(1, 0, 0, 1);
            }
        }
    }

    void OnPuzzleSolved() {
        game.timer_running = false;
        game.was_solved = true;
        game.last_solve_time = game.puzzle_timer;
        game.hint_level = 0; 
        current_blindfold_timer = 0.0f; // End blindfold on solve
        
        puzzle_status = "Puzzle Solved!";
        status_color = ImVec4(0, 1, 0, 1); 
        if (assets) assets->PlaySound(assets->snd_correct);
        
        game.effects.QueueEffect(EffectsManager::EffectType::FIREWORK, Square::underlying::NO_SQ, IM_COL32(255, 215, 0, 255));
        game.effects.AddVictory();
        
        // Record Stats BEFORE marking as solved (which clears current_puzzle)
        Puzzle* p = puzzle_manager.GetCurrentPuzzle();
        if (p) {
            // ONLY record stats if user solved it OR "Update DB" is checked in AutoSolve
            if (!auto_solve_active || auto_solve_update_db) {
                stats.RecordPuzzleResult(true, (float)game.puzzle_timer, game.puzzle_tti, p->themes, p->rating);
                game.last_total_time = puzzle_manager.MarkCurrentSolved(game.puzzle_timer);
            }
        }

        if (current_mode == PlayMode::STORM && !mode_game_over) {
            storm_score++;
            storm_timer += 2.0f;
            if (game.combo_count >= 5) storm_timer += 1.0f;
            
            // PB Broken logic
            if (storm_score > stats.state.arcade_records.best_storm_score && stats.state.arcade_records.best_storm_score > 0 && !session_pb_broken) {
                session_pb_broken = true;
                game.effects.QueueEffect(EffectsManager::EffectType::FIREWORK, Square::underlying::NO_SQ, IM_COL32(255, 215, 0, 255));
                game.effects.QueueEffect(EffectsManager::EffectType::FIREWORK, Square::underlying::NO_SQ, IM_COL32(255, 215, 0, 255));
                if (assets) assets->PlaySound(assets->snd_startup); // Epic sound
            } else if (game.combo_count > 0 && game.combo_count % 10 == 0) {
                // Milestone Combo Boom
            }

            game.auto_next = true;
            game.auto_next_timer = 0.5f;
        } else if (current_mode == PlayMode::STREAK && !mode_game_over) {
            streak_score++;
            
            // PB Broken logic
            if (streak_score > stats.state.arcade_records.best_streak_score && stats.state.arcade_records.best_streak_score > 0 && !session_pb_broken) {
                session_pb_broken = true;
                game.effects.QueueEffect(EffectsManager::EffectType::FIREWORK, Square::underlying::NO_SQ, IM_COL32(255, 215, 0, 255));
                game.effects.QueueEffect(EffectsManager::EffectType::FIREWORK, Square::underlying::NO_SQ, IM_COL32(255, 215, 0, 255));
                if (assets) assets->PlaySound(assets->snd_startup);
            } else if (streak_score > 0 && streak_score % 10 == 0) {
                // Streak Threshold Fireworks
                game.effects.QueueEffect(EffectsManager::EffectType::FIREWORK, Square::underlying::NO_SQ, IM_COL32(50, 255, 100, 255));
                game.effects.QueueEffect(EffectsManager::EffectType::FIREWORK, Square::underlying::NO_SQ, IM_COL32(50, 255, 100, 255));
                game.effects.QueueEffect(EffectsManager::EffectType::FIREWORK, Square::underlying::NO_SQ, IM_COL32(50, 255, 100, 255));
            }

            game.auto_next = true;
            game.auto_next_timer = 0.5f;
        } else if (game.auto_next) {
            game.auto_next_timer = game.auto_next_delay;
        } 
    }

    void HandlePuzzleLogic(Move move, bool auto_response = true) {
        game.hint_level = 0; 
        Puzzle* p = puzzle_manager.GetCurrentPuzzle();
        if (!p) {
            ExecuteMoveWithSound(move);
            return;
        }

        // Capture Time-to-Insight on first move
        if (!game.tti_captured) {
            game.puzzle_tti = (float)game.puzzle_timer;
            game.tti_captured = true;
        }

        std::string move_san = uci::moveToSan(game.board, move);

        if (puzzle_manager.current_move_idx < p->san_moves.size()) {
            std::string expected = p->san_moves[puzzle_manager.current_move_idx];
            
            bool match = (move_san == expected) || 
                         (move_san == expected + "+") || 
                         (move_san == expected + "#") ||
                         (expected == move_san + "+") ||
                         (expected == move_san + "#");

            if (match) {
                bool is_final_move = (puzzle_manager.current_move_idx + 1 >= p->san_moves.size());
                
                TriggerAnimation(move, true);
                ExecuteMoveWithSound(move, !is_final_move);
                game.effects.jiggle_timer = 0.3f; 

                puzzle_manager.current_move_idx++;
                game.combo_count++;
                if (game.combo_count > game.max_combo) game.max_combo = game.combo_count;
                if (game.combo_count > session_max_combo) session_max_combo = game.combo_count;
                
                if (game.forgiven_mistake_used && game.combo_count >= game.shield_recharge_target) {
                    game.forgiven_mistake_used = false;
                    puzzle_status = "Shield Recharged!";
                    status_color = ImVec4(0, 1, 0, 1); 
                }

                if (is_final_move) {
                    OnPuzzleSolved();
                } else if (auto_response) {
                    std::string opp_san = p->san_moves[puzzle_manager.current_move_idx];
                    Movelist response_moves;
                    movegen::legalmoves<>(response_moves, game.board);
                    Move opp_move = uci::parseSan(game.board, opp_san, response_moves); 
                    
                    if (opp_move == Move::NO_MOVE && (opp_san.back() == '+' || opp_san.back() == '#')) {
                         opp_move = uci::parseSan(game.board, opp_san.substr(0, opp_san.size()-1), response_moves);
                    }

                    if (opp_move != Move::NO_MOVE) {
                        TriggerAnimation(opp_move);
                        
                        bool is_opp_final = (puzzle_manager.current_move_idx + 1 >= p->san_moves.size());
                        
                        ExecuteMoveWithSound(opp_move, !is_opp_final);
                
                        puzzle_manager.current_move_idx++;
                        
                        if (is_opp_final) {
                            OnPuzzleSolved();
                        } 
                    } else {
                        puzzle_status = "Error: Opponent move failed (" + opp_san + ")";
                    }
                }
            } else {
                if (!game.forgiven_mistake_used && game.combo_count > 0) {
                    game.forgiven_mistake_used = true;
                    game.shield_recharge_target = game.combo_count + 20;
                    puzzle_status = "Mistake Forgiven! Combo Safe.";
                    status_color = ImVec4(1, 0.5, 0, 1); // Orange warning
                } else {
                    game.combo_count = 0;
                    game.forgiven_mistake_used = false;
                    puzzle_status = "Wrong Move! Try again.";
                    status_color = ImVec4(1, 0, 0, 1); 
                }

                game.timer_running = false;
                
                bool is_storm_or_streak = (current_mode == PlayMode::STORM || current_mode == PlayMode::STREAK);
                if (current_mode == PlayMode::STREAK && assets) assets->PlaySound(assets->snd_shatter);
                else if (assets) assets->PlaySound(assets->snd_wrong); 
                
                puzzle_manager.MarkCurrentFailed(is_storm_or_streak ? false : true, game.puzzle_timer); 
                game.effects.AddTrauma(0.8f); 
                game.effects.AddWobble();

                // Record Failure Stats
                stats.RecordPuzzleResult(false, (float)game.puzzle_timer, game.puzzle_tti, p->themes, p->rating);
                
                if (current_mode == PlayMode::STORM && !mode_game_over) {
                    game.auto_next = true;
                    game.auto_next_timer = 0.5f;
                } else if (current_mode == PlayMode::STREAK && !mode_game_over) {
                    streak_lives--;
                    if (streak_lives <= 0) {
                        streak_lives = 0;
                        mode_game_over = true;
                        if (stats.UpdateArcadeRecords(0, 0, streak_score)) stats.Save("Assets/user_data/stats.json");
                    } else {
                        game.auto_next = true;
                        game.auto_next_timer = 0.5f;
                    }
                }
            }
        }
    }
    
    // --- PGN Parsing ---
    
    bool ImportPGN(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) return false;

        std::stringstream buffer;
        buffer << file.rdbuf();
        return ImportPGNFromText(buffer.str());
    }

    bool ImportPGNFromText(std::string content) {
        if (content.empty()) return false;
        imported_games.clear();
        current_game_idx = -1;

        // Split by game blocks (start of a header at start of line)
        std::vector<std::string> game_blocks;
        std::regex gameStartRegex(R"(^\[Event )", std::regex_constants::multiline);
        auto it = std::sregex_token_iterator(content.begin(), content.end(), gameStartRegex, -1);
        auto end = std::sregex_token_iterator();
        
        bool first = true;
        for (; it != end; ++it) {
            std::string block = it->str();
            if (first) { 
                // The first token might be empty or junk before the first [Event
                if (block.find("[") == std::string::npos) { first = false; continue; }
            }
            // Re-add the [Event that was consumed by splitting if it's not the first part
            if (!first) block = "[Event " + block;
            game_blocks.push_back(block);
            first = false;
        }
        
        // If regex split failed to find multiple, treat whole content as one block
        if (game_blocks.empty()) game_blocks.push_back(content);

        for (auto& block : game_blocks) {
            ParsedGame pg;
            
            auto ExtractHeader = [&](std::string key) {
                std::regex h(R"(\[)" + key + R"( \"([^\"]*)\"\])");
                std::smatch m;
                if (std::regex_search(block, m, h)) return m[1].str();
                return std::string("");
            };

            pg.white_player = ExtractHeader("White");
            pg.black_player = ExtractHeader("Black");
            pg.date = ExtractHeader("Date");
            pg.event = ExtractHeader("Event");
            pg.result = ExtractHeader("Result");
            pg.start_fen = ExtractHeader("FEN");
            if (pg.start_fen.empty()) pg.start_fen = constants::STARTPOS;
            if (pg.white_player.empty()) pg.white_player = "White";
            if (pg.black_player.empty()) pg.black_player = "Black";

            // Remove all headers to get moves
            std::regex headerRegex(R"(\[[^\]]*\])");
            std::string move_text = std::regex_replace(block, headerRegex, " ");

            // Clean moves
            std::regex commentRegex(R"(\{[^\}]*\})");
            move_text = std::regex_replace(move_text, commentRegex, " ");
            std::regex varRegex(R"(\([^\)]*\))");
            move_text = std::regex_replace(move_text, varRegex, " ");
            std::regex resultRegex(R"(1-0|0-1|1/2-1/2|\*)");
            move_text = std::regex_replace(move_text, resultRegex, " ");

            std::stringstream ss(move_text);
            std::string token;
            Board temp_board(pg.start_fen);
            
            while (ss >> token) {
                // Skip move numbers
                if (isdigit(token[0])) {
                    bool is_move_num = true;
                    for (size_t i = 1; i < token.length(); ++i) {
                        if (!isdigit(token[i]) && token[i] != '.') { is_move_num = false; break; }
                    }
                    if (is_move_num) continue;
                }

                Movelist moves;
                movegen::legalmoves<>(moves, temp_board);
                Move m = uci::parseSan(temp_board, token, moves);
                if (m == Move::NO_MOVE && (token.back() == '+' || token.back() == '#')) {
                    m = uci::parseSan(temp_board, token.substr(0, token.size()-1), moves);
                }

                if (m != Move::NO_MOVE) {
                    pg.san_history.push_back(uci::moveToSan(temp_board, m));
                    pg.move_history.push_back(m);
                    temp_board.makeMove(m);
                } else {
                    break; 
                }
            }
            if (!pg.move_history.empty() || pg.start_fen != constants::STARTPOS) {
                imported_games.push_back(pg);
            }
        }

        if (!imported_games.empty()) {
            LoadImportedGame(0);
            return true;
        }
        
        return false;
    }

    // --- Analysis ---
    
    enum class AnalysisMode { QUICK, DEEP, SMART_HYBRID };
    AnalysisMode analysis_mode = AnalysisMode::SMART_HYBRID;
    int analysis_pass = 1; // 1 = Quick/Initial, 2 = Deepening
    std::vector<int> deep_indices; 

    void StartFullGameAnalysis(AnalysisMode mode = AnalysisMode::SMART_HYBRID) {
        if (game.move_tree.empty()) return;
        
        analysis_mode = mode;
        analysis_active = true;
        analysis_idx = 0;
        analysis_pass = 1;
        deep_indices.clear();
        game.jump_to_node(-1); 
        analysis_waiting = false;
        
        game.sf_active = true;
        // Pass 1 depth
        game.sf_engine.limit_depth = (analysis_mode == AnalysisMode::DEEP) ? 24 : 14; 
        game.UpdateEngines();
    }
    
    void StopFullGameAnalysis() {
        analysis_active = false;
        analysis_waiting = false;
        game.sf_engine.StopAnalysis();
    }
    
    void UpdateAnalysis() {
        if (!analysis_active) return;
        
        if (game.sf_engine.IsSearching()) return;

        auto main_branch = game.get_main_branch_indices();

        if (analysis_waiting) {
            auto info = game.sf_engine.GetInfo();
            
            float eval = info.score_cp / 100.0f;
            std::string eval_str;
            
            if (info.score_mate != 0) {
                eval = (info.score_mate > 0 ? 20.0f : -20.0f);
                eval_str = "M" + std::to_string(std::abs(info.score_mate));
                if (info.score_mate < 0) eval_str = "-" + eval_str;
            } else {
                std::stringstream ss;
                ss << std::fixed << std::setprecision(2) << eval;
                eval_str = (eval > 0 ? "+" : "") + ss.str();
            }
            
            Board temp(game.start_fen);
            for(int i=0; i<analysis_idx && i < main_branch.size(); ++i) {
                temp.makeMove(game.move_tree[main_branch[i]].move);
            }

            if (temp.sideToMove() == Color::BLACK) eval = -eval;
            
            if (analysis_idx < (int)main_branch.size()) {
                int node_idx = main_branch[analysis_idx];
                float old_eval = game.move_tree[node_idx].eval;
                game.move_tree[node_idx].eval = eval;
                game.move_tree[node_idx].eval_str = eval_str;

                // Identify critical moments during Pass 1
                if (analysis_pass == 1 && analysis_mode == AnalysisMode::SMART_HYBRID) {
                    if (analysis_idx > 0) {
                        float shift = std::abs(eval - game.move_tree[main_branch[analysis_idx - 1]].eval);
                        if (shift > 0.5f || info.score_mate != 0) {
                            deep_indices.push_back(analysis_idx);
                        }
                    }
                }
            }
            
            if (analysis_pass == 1) {
                game.jump_to_node(main_branch[analysis_idx]);
                analysis_idx++;
            } else {
                // Pass 2: moves through deep_indices
                if (!deep_indices.empty()) {
                    deep_indices.erase(deep_indices.begin());
                }
            }
            analysis_waiting = false;
        }

        // 2. Start next analysis
        if (analysis_pass == 1) {
            if (analysis_idx < (int)main_branch.size()) {
                Board temp(game.start_fen);
                for(int i=0; i<analysis_idx && i < main_branch.size(); ++i) {
                    temp.makeMove(game.move_tree[main_branch[i]].move);
                }
                game.sf_engine.StartAnalysis(temp.getFen());
                analysis_waiting = true;
            } else {
                // Pass 1 finished
                if (analysis_mode == AnalysisMode::SMART_HYBRID && !deep_indices.empty()) {
                    analysis_pass = 2;
                    game.sf_engine.limit_depth = 24;
                    puzzle_status = "Deepening Analysis...";
                } else {
                    StopFullGameAnalysis();
                    puzzle_status = "Analysis Complete";
                }
            }
        } else if (analysis_pass == 2) {
            if (!deep_indices.empty()) {
                analysis_idx = deep_indices[0];
                game.jump_to_node(main_branch[analysis_idx]);
                
                Board temp(game.start_fen);
                for(int i=0; i<analysis_idx && i < main_branch.size(); ++i) {
                    temp.makeMove(game.move_tree[main_branch[i]].move);
                }
                game.sf_engine.StartAnalysis(temp.getFen());
                analysis_waiting = true;
            } else {
                StopFullGameAnalysis();
                puzzle_status = "Analysis Complete (Deep)";
            }
        }
    }

    // --- Batch Analysis ---

    void StartBatchAnalysis(int depth = 12, bool deep_dive = true) {
        if (imported_games.empty()) return;
        batch.active = true;
        batch.pass = 1;
        batch.current_game_idx = 0;
        batch.current_move_idx = 0;
        batch.waiting_for_engine = false;
        batch.scout_depth = depth;
        batch.enable_deep_dive = deep_dive;
        
        batch.report = BatchReport();
        batch.report.total_games = (int)imported_games.size();
        batch.report.feed_log.push_back("Started Scout Pass (Depth " + std::to_string(depth) + ")...");
        
        game.sf_active = true;
        game.sf_engine.limit_depth = depth;
        game.UpdateEngines();
    }

    void StopBatchAnalysis() {
        batch.active = false;
        game.sf_engine.StopAnalysis();
        batch.report.feed_log.push_back("Batch Analysis Halted.");
    }

    void UpdateBatchAnalysis() {
        if (!batch.active) return;
        if (game.sf_engine.IsSearching()) return;

        if (batch.waiting_for_engine) {
            auto info = game.sf_engine.GetInfo();
            float eval = info.score_cp / 100.0f;
            if (info.score_mate != 0) eval = (info.score_mate > 0 ? 20.0f : -20.0f);
            
            const auto& pg = imported_games[batch.current_game_idx];
            
            if (batch.pass == 1) {
                if (batch.current_move_idx == 0) {
                    GameSummary gs;
                    gs.game_idx = batch.current_game_idx;
                    gs.white = pg.white_player;
                    gs.black = pg.black_player;
                    
                    // Pre-calculate cached names for UI efficiency
                    auto FormatName = [&](std::string name) -> std::string {
                        if (!pov_player_name.empty()) {
                            std::string lower_name = name;
                            std::string lower_pov = pov_player_name;
                            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
                            std::transform(lower_pov.begin(), lower_pov.end(), lower_pov.begin(), ::tolower);
                            if (lower_name.find(lower_pov) != std::string::npos) return "You";
                        }
                        if (name.length() > 3) return name.substr(0, 3);
                        return name;
                    };
                    gs.white_name_cached = FormatName(gs.white);
                    gs.black_name_cached = FormatName(gs.black);

                    batch.report.summaries.push_back(gs);
                    batch.last_eval = 0.0f; 
                }

                auto& gs = batch.report.summaries.back();
                
                Board temp(pg.start_fen);
                for(int i=0; i<batch.current_move_idx; ++i) temp.makeMove(pg.move_history[i]);
                
                // Normalize eval to White's perspective
                float eval_white = (temp.sideToMove() == Color::WHITE) ? eval : -eval;

                if (batch.current_move_idx > 0) {
                    // Identify who just played
                    Color side_that_played = (temp.sideToMove() == Color::BLACK) ? Color::WHITE : Color::BLACK;

                    // Get scores from the perspective of the player who made the move
                    float score_now = (side_that_played == Color::WHITE) ? eval_white : -eval_white;
                    float score_prev = (side_that_played == Color::WHITE) ? batch.last_eval : -batch.last_eval;

                    float win_now = batch.toWinPercent(score_now);
                    float win_before = batch.toWinPercent(score_prev);
                    float win_loss = std::max(0.0f, win_before - win_now);
                    float move_acc = batch.calcAccuracy(win_loss);

                    int quality = 0;
                    
                    float shift = std::abs(score_now - score_prev); // Absolute change
                    
                    // ACPL
                    float cp_loss = std::max(0.0f, score_prev - score_now);
                    gs.acpl += cp_loss * 100.0f;

                    // Categorization
                    if (win_loss > 20.0f) { quality = 3; gs.blunders++; batch.report.total_blunders++; gs.deserves_deep_dive = true; }
                    else if (win_loss > 10.0f) { quality = 2; gs.mistakes++; }
                    else if (win_loss > 5.0f) { quality = 1; gs.inaccuracies++; }
                    else if (win_loss < 0.5f) { 
                        if (std::abs(score_now) < 2.0f) { quality = 4; gs.greats++; }
                        if (win_loss < 0.1f && std::abs(score_now) < 1.0f) { quality = 5; gs.brilliants++; batch.report.total_brilliants++; }
                    }

                    // Punishment logic: if opponent's last move was a blunder
                    static float last_opp_win_loss = 0.0f;
                    if (last_opp_win_loss > 15.0f) {
                        gs.punishment += move_acc;
                    }
                    // Update for next move (which will be opponent's)
                    last_opp_win_loss = win_loss;

                    // Fighting Spirit (if we were losing but played well)
                    if (score_prev < -2.0f && score_prev > -10.0f) {
                        gs.fighting_spirit += move_acc;
                    }

                    // Phase
                    if (batch.current_move_idx <= 10) gs.opAcc += move_acc;
                    else if (batch.getMaterial(temp) < 28) gs.endAcc += move_acc;
                    else gs.midAcc += move_acc;

                    gs.accuracy += move_acc;
                    
                    gs.move_qualities.push_back(quality);
                    // Store normalized eval for consistency in graphs/logs
                    gs.move_evals.push_back(eval_white);
                } else {
                    // For the start position (or move 0), we just record the eval
                     gs.move_evals.push_back(eval_white);
                }
                
                // Update last_eval to be the current position's score (White's perspective)
                batch.last_eval = eval_white;
            }

            batch.current_move_idx++;
            batch.waiting_for_engine = false;

            if (batch.current_move_idx > (int)pg.move_history.size()) {
                auto& gs = batch.report.summaries.back();
                int n = (int)pg.move_history.size();
                if (n > 0) {
                    gs.accuracy /= (float)n;
                    gs.acpl /= (float)n;
                    // Note: Phase accuracies would need counts to be perfect, 
                    // but this is a batch overview. We'll simplify UI display.
                }

                batch.current_game_idx++;
                batch.current_move_idx = 0;
                batch.report.games_scouted++;
                
                if (batch.pass == 1) {
                    batch.report.feed_log.push_back("Analyzed " + gs.white + " (" + std::to_string((int)gs.accuracy) + "%)");
                }

                if (batch.current_game_idx >= (int)imported_games.size()) {
                    if (batch.pass == 1 && batch.enable_deep_dive) {
                        batch.pass = 2;
                        batch.current_game_idx = 0;
                        batch.report.feed_log.push_back("--- Starting Deep Dive on Complex Games ---");
                        game.sf_engine.limit_depth = 22;
                    } else {
                        batch.active = false;
                        batch.report.feed_log.push_back("--- Batch Complete ---");
                        return;
                    }
                }
            }
        }

        if (batch.active) {
            const auto& current_game = imported_games[batch.current_game_idx];
            if (batch.pass == 2) {
                bool skip = true;
                for (const auto& gs : batch.report.summaries) {
                    if (gs.game_idx == batch.current_game_idx && gs.deserves_deep_dive) { skip = false; break; }
                }
                if (skip) {
                    batch.current_game_idx++;
                    batch.current_move_idx = 0;
                    if (batch.current_game_idx >= (int)imported_games.size()) batch.active = false;
                    return;
                }
            }

            Board temp(current_game.start_fen);
            for (int i = 0; i < batch.current_move_idx; ++i) temp.makeMove(current_game.move_history[i]);
            game.sf_engine.StartAnalysis(temp.getFen());
            batch.waiting_for_engine = true;
        }
    }

    void UpdateAutoSolve(float dt) {
        if (!auto_solve_active) return;
        
        if (move_anim.active) return; 
        
        Puzzle* p = puzzle_manager.GetCurrentPuzzle();
        if (!p || game.was_solved) {
            if (auto_solve_state != AutoSolveState::WAITING) {
                auto_solve_state = AutoSolveState::WAITING;
                float delay = 2.5f;
                if (auto_solve_speed == AutoSolveSpeed::FAST) delay = 1.0f;
                else if (auto_solve_speed == AutoSolveSpeed::SLOW) delay = 4.0f;
                else if (auto_solve_speed == AutoSolveSpeed::CUSTOM) delay = auto_solve_custom_delay;
                auto_solve_timer = delay;
            } else {
                auto_solve_timer -= dt;
                if (auto_solve_timer <= 0.0f) {
                    if (game.review_mode) StartPuzzle(puzzle_manager.GetNextReviewPuzzle());
                    else StartPuzzle(puzzle_manager.GetRandomPuzzle());
                    
                    float delay = 1.5f;
                    if (auto_solve_speed == AutoSolveSpeed::FAST) delay = 0.5f;
                    else if (auto_solve_speed == AutoSolveSpeed::SLOW) delay = 3.0f;
                    else if (auto_solve_speed == AutoSolveSpeed::CUSTOM) delay = auto_solve_custom_delay * 0.5f; 
                    auto_solve_timer = delay;
                }
            }
            return;
        }

        switch (auto_solve_state) {
            case AutoSolveState::WAITING:
                auto_solve_timer -= dt;
                if (auto_solve_timer <= 0.0f) {
                    if (auto_solve_show_hint) {
                        auto_solve_state = AutoSolveState::HINTING;
                        game.hint_level = 1;
                        auto_solve_timer = 0.8f; 
                    } else {
                        auto_solve_state = AutoSolveState::PLAYER_MOVING;
                    }
                }
                break;
                
            case AutoSolveState::HINTING:
                auto_solve_timer -= dt;
                if (auto_solve_timer <= 0.0f) {
                    auto_solve_state = AutoSolveState::PLAYER_MOVING;
                    game.hint_level = 0;
                }
                break;
                
            case AutoSolveState::PLAYER_MOVING:
                if (puzzle_manager.current_move_idx < (int)p->san_moves.size()) {
                    std::string expected_san = p->san_moves[puzzle_manager.current_move_idx];
                    Movelist moves;
                    movegen::legalmoves<>(moves, game.board);
                    Move m = uci::parseSan(game.board, expected_san, moves);
                    
                    if (m == Move::NO_MOVE && (expected_san.back() == '+' || expected_san.back() == '#')) {
                        m = uci::parseSan(game.board, expected_san.substr(0, expected_san.size()-1), moves);
                    }

                    if (m != Move::NO_MOVE) {
                        HandlePuzzleLogic(m, false);
                        
                        if (game.was_solved) {
                             auto_solve_state = AutoSolveState::WAITING;
                             float delay = 2.5f;
                             if (auto_solve_speed == AutoSolveSpeed::FAST) delay = 1.0f;
                             else if (auto_solve_speed == AutoSolveSpeed::SLOW) delay = 4.0f;
                             else if (auto_solve_speed == AutoSolveSpeed::CUSTOM) delay = auto_solve_custom_delay;
                             auto_solve_timer = delay;
                        } else {
                            auto_solve_state = AutoSolveState::WAITING_OPPONENT;
                            float delay = 0.8f;
                            if (auto_solve_speed == AutoSolveSpeed::FAST) delay = 0.4f;
                            else if (auto_solve_speed == AutoSolveSpeed::SLOW) delay = 1.5f;
                            else if (auto_solve_speed == AutoSolveSpeed::CUSTOM) delay = auto_solve_custom_delay * 0.3f; 
                            auto_solve_timer = delay;
                        }
                    } else {
                        auto_solve_active = false;
                        puzzle_status = "Auto-Solve Error: Invalid Move";
                    }
                }
                break;

            case AutoSolveState::WAITING_OPPONENT:
                auto_solve_timer -= dt;
                if (auto_solve_timer <= 0.0f) {
                    auto_solve_state = AutoSolveState::OPPONENT_MOVING;
                }
                break;

            case AutoSolveState::OPPONENT_MOVING:
                if (puzzle_manager.current_move_idx < (int)p->san_moves.size()) {
                    std::string opp_san = p->san_moves[puzzle_manager.current_move_idx];
                    Movelist response_moves;
                    movegen::legalmoves<>(response_moves, game.board);
                    Move opp_move = uci::parseSan(game.board, opp_san, response_moves); 
                    
                    if (opp_move == Move::NO_MOVE && (opp_san.back() == '+' || opp_san.back() == '#')) {
                         opp_move = uci::parseSan(game.board, opp_san.substr(0, opp_san.size()-1), response_moves);
                    }

                    if (opp_move != Move::NO_MOVE) {
                        TriggerAnimation(opp_move);
                        bool is_opp_final = (puzzle_manager.current_move_idx + 1 >= p->san_moves.size());
                        ExecuteMoveWithSound(opp_move, !is_opp_final);
                        puzzle_manager.current_move_idx++;
                        
                        if (is_opp_final) {
                            OnPuzzleSolved();
                             auto_solve_state = AutoSolveState::WAITING;
                             float delay = 2.5f;
                             if (auto_solve_speed == AutoSolveSpeed::FAST) delay = 1.0f;
                             else if (auto_solve_speed == AutoSolveSpeed::SLOW) delay = 4.0f;
                             else if (auto_solve_speed == AutoSolveSpeed::CUSTOM) delay = auto_solve_custom_delay;
                             auto_solve_timer = delay;
                        } else {
                            auto_solve_state = AutoSolveState::WAITING;
                            float delay = 1.5f;
                            if (auto_solve_speed == AutoSolveSpeed::FAST) delay = 0.5f;
                            else if (auto_solve_speed == AutoSolveSpeed::SLOW) delay = 2.5f;
                            else if (auto_solve_speed == AutoSolveSpeed::CUSTOM) delay = auto_solve_custom_delay;
                            auto_solve_timer = delay;
                        }
                    } else {
                        puzzle_status = "Error: Opponent move failed (" + opp_san + ")";
                        auto_solve_active = false;
                    }
                }
                break;
        }
    }

    void Update(float dt) {
        idle_activity_timer += dt;
        
        if (game.opening_notif_active) {
            game.opening_notif_timer -= dt;
            if (game.opening_notif_timer <= 0.0f) game.opening_notif_active = false;
        }
        
        // Treat system-driven events as "Activity" to prevent 1 FPS mode
        bool is_system_active = auto_solve_active || move_anim.active || analysis_active || 
                                batch.active || (game.auto_next_timer >= 0) || 
                                (current_mode == PlayMode::STORM && !mode_game_over);
        if (is_system_active) {
            idle_activity_timer = 0.0f;
        }

        sys_monitor.Update(dt);
        UpdateAnalysis();
        UpdateBatchAnalysis();
        UpdateAutoSolve(dt);
        
        if (game.current_node_idx != -1) {
            auto FormatLiveEval = [&](EngineManager& eng) -> std::string {
                if (!eng.IsRunning()) return "";
                auto info = eng.GetInfo();
                if (info.lines.empty()) return "";
                auto best_line = info.lines.begin()->second;
                if (best_line.score_mate != 0) {
                    return "M" + std::to_string(std::abs(best_line.score_mate));
                } else {
                    float cp = best_line.score_cp / 100.0f;
                    if (game.board.sideToMove() == chess::Color::BLACK) cp = -cp;
                    std::stringstream ss;
                    ss << std::fixed << std::setprecision(1) << std::abs(cp);
                    return (cp > 0 ? "+" : (cp < 0 ? "-" : "")) + ss.str();
                }
            };
            if (game.sf_active) game.move_tree[game.current_node_idx].live_sf_eval = FormatLiveEval(game.sf_engine);
            if (game.lc0_active) game.move_tree[game.current_node_idx].live_lc0_eval = FormatLiveEval(game.lc0_engine);
        }
        
        if (current_mode == PlayMode::STORM && !mode_game_over) {
            storm_timer -= dt;
            if (storm_timer <= 0.0f) {
                storm_timer = 0.0f;
                mode_game_over = true;
                game.timer_running = false;
                if (stats.UpdateArcadeRecords(storm_score, session_max_combo, 0)) stats.Save("Assets/user_data/stats.json");
            } else if (storm_timer <= 15.0f && assets) {
                tick_timer -= dt;
                if (tick_timer <= 0.0f) {
                    assets->PlayTickSound();
                    tick_timer = 1.0f;
                }
            }
        }
        
        // Blindfold Timer Countdown
        if (blindfold_active && blindfold_use_timer && current_blindfold_timer > 0.0f) {
            current_blindfold_timer -= dt;
            if (current_blindfold_timer < 0.0f) current_blindfold_timer = 0.0f;
        }

        game.effects.Update(dt, game.combo_count);

        float target_flip = game.is_flipped ? 1.0f : 0.0f;
        if (game.effects.enable_rotation && !game.effects.disable_all_effects) {
            if (std::abs(game.flip_val - target_flip) > 0.001f) {
                game.flip_val += (target_flip - game.flip_val) * dt * 5.0f;
            } else {
                game.flip_val = target_flip;
            }
        } else {
            game.flip_val = target_flip;
        }

        if (game.auto_next && game.auto_next_timer >= 0.0f) {
            game.auto_next_timer -= dt;
            if (game.auto_next_timer <= 0.0f) {
                game.auto_next_timer = -1.0f;
                if (current_mode == PlayMode::STORM) {
                    if (!mode_game_over) StartPuzzle(puzzle_manager.GetProgressivePuzzle(storm_score));
                } else if (current_mode == PlayMode::STREAK) {
                    if (!mode_game_over) StartPuzzle(puzzle_manager.GetProgressivePuzzle(streak_score));
                } else if (game.review_mode) {
                    StartPuzzle(puzzle_manager.GetNextReviewPuzzle());
                } else {
                    StartPuzzle(puzzle_manager.GetRandomPuzzle());
                }
            }
        }
        
        if (game.timer_running) {
            game.puzzle_timer += dt;
        }

        if (game.preview_mode && game.preview_animating) {
            if (!move_anim.active) {
                if (game.preview_anim_idx < (int)game.preview_moves.size()) {
                    Move m = game.preview_moves[game.preview_anim_idx];
                    TriggerAnimation(m);
                    if (assets) assets->PlaySound(assets->snd_move); 
                    game.preview_anim_idx++;
                } else {
                    game.preview_animating = false;
                }
            }
        }

        if (play_vs_engine && game.board.isGameOver().second == chess::GameResult::NONE && !move_anim.active) {
            Color engine_color = game.is_flipped ? Color::WHITE : Color::BLACK;
            if (game.board.sideToMove() == engine_color) {
                engine_move_timer += dt;
                auto info = game.sf_engine.GetInfo();
                
                if ((engine_move_timer > 1.0f && info.depth >= 10 && !info.best_move.empty()) || engine_move_timer > 3.0f) {
                    if (!info.best_move.empty()) {
                        chess::Move best = chess::uci::uciToMove(game.board, info.best_move);
                        if (best != chess::Move::NO_MOVE) {
                            TriggerAnimation(best);
                            ExecuteMoveWithSound(best);
                        }
                    }
                    engine_move_timer = 0.0f;
                }
            } else {
                engine_move_timer = 0.0f;
            }
        } else {
            engine_move_timer = 0.0f;
        }

        // Auto Play Logic
        if (game.auto_play_active) {
            auto game_result = game.board.isGameOver().second;
            if (game_result != chess::GameResult::NONE) {
                if (!game.game_over_recorded) {
                    std::string winner_str = "Draw";
                    if (game_result == chess::GameResult::LOSE) {
                        if (game.board.sideToMove() == Color::WHITE) {
                            game.ap_wins_black++;
                            winner_str = "BlackWins";
                            std::string winner = (game.auto_play_black == GameState::AutoPlayPlayer::HUMAN) ? "Player" : ((game.auto_play_black == GameState::AutoPlayPlayer::STOCKFISH) ? "Stockfish" : "Lc0");
                            game.banner_text = winner + " (Black) Wins!";
                            game.banner_color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
                        } else {
                            game.ap_wins_white++;
                            winner_str = "WhiteWins";
                            std::string winner = (game.auto_play_white == GameState::AutoPlayPlayer::HUMAN) ? "Player" : ((game.auto_play_white == GameState::AutoPlayPlayer::STOCKFISH) ? "Stockfish" : "Lc0");
                            game.banner_text = winner + " (White) Wins!";
                            game.banner_color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
                        }
                    } else if (game_result == chess::GameResult::DRAW) {
                        game.ap_draws++;
                        game.banner_text = "Match Drawn!";
                        game.banner_color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                    }
                    game.banner_active = true;
                    game.banner_timer = 4.0f; // Show banner for 4 seconds
                    game.game_over_recorded = true;
                    
                    if (game.auto_play_save_pgn) {
                        game.SaveAutoplayPGN(winner_str);
                    }
                }

                if (game.banner_active) {
                    game.banner_timer -= dt;
                    if (game.banner_timer <= 0.0f) game.banner_active = false;
                }

                if (game.auto_play_restart) {
                    game.auto_play_end_timer += dt;
                    if (game.auto_play_end_timer >= 5.0f) {
                        // Reset Game
                        auto opening = game.GetSelectedOpening();
                        game.reset(opening.second);
                        game.auto_play_end_timer = 0.0f;
                        game.auto_play_timer = 0.0f;
                        
                        // Trigger opening notification
                        game.opening_notif_active = true;
                        game.opening_notif_text = "Opening: " + opening.first;
                        game.opening_notif_timer = 3.0f;
                    }
                }
            } else if (!move_anim.active && !game.preview_animating) {
                game.auto_play_end_timer = 0.0f; // Reset end timer if game is running
                
                if (game.opening_notif_active) {
                    game.opening_notif_timer -= dt;
                    if (game.opening_notif_timer <= 0.0f) game.opening_notif_active = false;
                }
                
                Color side = game.board.sideToMove();
                GameState::AutoPlayPlayer current_player = (side == Color::WHITE) ? game.auto_play_white : game.auto_play_black;
                
                if (current_player != GameState::AutoPlayPlayer::HUMAN) {
                    EngineManager* eng = nullptr;
                    bool is_active = false;

                    if (game.auto_play_use_separate_instances) {
                        if (current_player == GameState::AutoPlayPlayer::STOCKFISH) {
                            eng = (side == Color::WHITE) ? &game.sf_engine_white : &game.sf_engine_black;
                        } else {
                            eng = (side == Color::WHITE) ? &game.lc0_engine_white : &game.lc0_engine_black;
                        }
                        // Assume dedicated engines are always "active" when used this way, we start them if they aren't running
                        is_active = true; 

                        if (!eng->IsRunning()) {
                            // Find paths
                            auto FindAsset = [](std::string path) -> std::string {
                                if (std::filesystem::exists(path)) return std::filesystem::absolute(path).string();
                                if (std::filesystem::exists("../" + path)) return std::filesystem::absolute("../" + path).string();
                                if (std::filesystem::exists("../../" + path)) return std::filesystem::absolute("../../" + path).string();
                                return "";
                            };
                            if (current_player == GameState::AutoPlayPlayer::STOCKFISH) {
                                std::string path = game.FindEngineExe("Assets/Engines/stockfish");
                                if (path.empty()) path = "stockfish";
                                eng->StartEngine("\"" + path + "\"");
                            } else {
                                std::string exe_path = game.FindEngineExe("Assets/Engines/lc0");
                                std::string w_path = game.lc0_engine.weights_path;
                                if (w_path.empty()) w_path = game.FindLc0Weights("Assets/Engines/lc0");
                                else if (std::filesystem::exists(w_path)) w_path = std::filesystem::absolute(w_path).string();
                                else { std::string found = FindAsset(w_path); if (!found.empty()) w_path = found; }
                                std::string work_dir = "";
                                if (!exe_path.empty()) work_dir = std::filesystem::path(exe_path).parent_path().string();
                                if (!exe_path.empty() && !w_path.empty()) {
                                    eng->StartEngine("\"" + exe_path + "\" --weights=\"" + w_path + "\" --backend=" + game.lc0_engine.backend, work_dir);
                                }
                            }
                        }
                    } else {
                        eng = (current_player == GameState::AutoPlayPlayer::STOCKFISH) ? &game.sf_engine : &game.lc0_engine;
                        is_active = (current_player == GameState::AutoPlayPlayer::STOCKFISH) ? game.sf_active : game.lc0_active;
                    }

                    if (is_active) {
                        game.auto_play_timer += dt;

                        // Ensure engine is analyzing the current position if using dedicated instances
                        if (game.auto_play_use_separate_instances && game.auto_play_last_search_fen != game.board.getFen()) {
                            if (current_player == GameState::AutoPlayPlayer::STOCKFISH) eng->limit_depth = (side == Color::WHITE) ? game.auto_play_white_depth : game.auto_play_black_depth;
                            else eng->limit_nodes = (side == Color::WHITE) ? game.auto_play_white_nodes : game.auto_play_black_nodes;
                            game.auto_play_last_search_fen = game.board.getFen();
                            eng->StartAnalysis(game.auto_play_last_search_fen);
                        }

                        auto info = eng->GetInfo();

                        bool use_time = (side == Color::WHITE) ? game.auto_play_white_use_time : game.auto_play_black_use_time;
                        float move_time = (side == Color::WHITE) ? game.auto_play_white_move_time : game.auto_play_black_move_time;

                        bool time_met = !use_time || (game.auto_play_timer >= move_time);
                        bool engine_done = !eng->IsSearching();

                        // Fallback to force move if it takes too long
                        if (!use_time && game.auto_play_timer > 15.0f && info.depth >= 1 && !info.best_move.empty()) engine_done = true;
                        if (use_time && game.auto_play_timer > move_time + 10.0f && info.depth >= 1 && !info.best_move.empty()) engine_done = true;

                        if (time_met && engine_done && !info.best_move.empty()) {
                            if (eng->IsSearching()) eng->StopAnalysis();

                            chess::Move best = chess::uci::uciToMove(game.board, info.best_move);
                            if (best != chess::Move::NO_MOVE) {
                                TriggerAnimation(best);
                                ExecuteMoveWithSound(best);
                            }
                            game.auto_play_timer = 0.0f;
                        }
                    } else {
                        // Activate engine if not active (only applies to shared instances)
                        if (!game.auto_play_use_separate_instances) {
                            if (current_player == GameState::AutoPlayPlayer::STOCKFISH) { game.sf_active = true; game.UpdateEngines(true); }
                            if (current_player == GameState::AutoPlayPlayer::LC0) { game.lc0_active = true; game.UpdateEngines(true); }
                        }
                    }
                } else {                    game.auto_play_timer = 0.0f; // Reset if human's turn
                }
            }
        } else {
            game.auto_play_timer = 0.0f;
            game.auto_play_end_timer = 0.0f;
        }
    }

    void ParseUCI(const std::string& uci_string) { }
    void SetBlindfoldMode(bool active) { }
    void HandleKeyboardInput(std::string san) {
        if (san.empty()) return;
        
        // Auto-capitalize piece letters and castling for QoL
        if (san[0] == 'q' || san[0] == 'r' || san[0] == 'b' || san[0] == 'n' || san[0] == 'k' || san[0] == 'o') {
            san[0] = toupper(san[0]);
        }
        for (size_t i = 1; i < san.size(); ++i) {
            if (san[0] == 'O' && san[i] == 'o') san[i] = 'O';
        }
        
        Move m = Move::NO_MOVE;
        try {
            Movelist moves;
            movegen::legalmoves<>(moves, game.board);
            m = uci::parseSan(game.board, san, moves);
            
            // Fallback for missing + or #
            if (m == Move::NO_MOVE) {
                try { m = uci::parseSan(game.board, san + "+", moves); } catch (...) {}
                if (m == Move::NO_MOVE) {
                    try { m = uci::parseSan(game.board, san + "#", moves); } catch (...) {}
                }
            }
        } catch (...) {
            m = Move::NO_MOVE;
        }

        if (m != Move::NO_MOVE) {
            // If we're in a puzzle, route through HandlePuzzleLogic
            if (current_mode == PlayMode::STORM || current_mode == PlayMode::STREAK || current_mode == PlayMode::REVIEW || (!game.review_mode && puzzle_manager.GetCurrentPuzzle())) {
                HandlePuzzleLogic(m, true);
            } else {
                // Free Play or vs Engine
                TriggerAnimation(m);
                ExecuteMoveWithSound(m);
            }
        } else {
            // Invalid move feedback
            puzzle_status = "Invalid Move: " + san;
            status_color = ImVec4(1, 0, 0, 1);
            if (assets) assets->PlaySound(assets->snd_wrong);
            game.effects.AddWobble();
        }
    }
    void SwitchUserProfile(const std::string& profile_name) { }
    void ExportStatistics(const std::string& format) { }
};
