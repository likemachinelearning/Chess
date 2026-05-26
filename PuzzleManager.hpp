#pragma once
#include <string>
#include <vector>
#include <thread>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <unordered_set>
#include <set>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <map>
#include <nlohmann/json.hpp>
#include "chess.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

struct Puzzle {
    std::string fen;
    std::vector<std::string> san_moves;
    std::string rating;
    std::string themes;
    std::string id;
    std::string source_file;
    std::string completion_date; // YYYY-MM-DD (Last Reviewed)
    std::string first_seen_date; // YYYY-MM-DD (First Attempt)
    
    // SRS Fields
    int srs_stage = 0; 
    int64_t next_review_time = 0; // Unix Timestamp
    int fails_remaining = 0; 
    double ease_factor = 2.5; // Default Ease (SM-2)
    int64_t last_interval = 0; // Last interval in seconds
    double total_seconds_spent = 0.0; // Cumulative time across attempts
};

struct HistoryAction {
    Puzzle p;
    int original_index;
};

class PuzzleManager {
public:
    std::vector<Puzzle> active_puzzles;
    std::vector<Puzzle> user_puzzles; // Merged history
    std::unordered_set<std::string> user_ids;
    
    // Dynamic Chunking Cache
    std::map<int, std::vector<Puzzle>> loaded_chunks;
    std::unordered_set<std::string> session_used_ids;
    int current_chunk_bucket = -1;
    
    // Undo/Redo Stacks
    std::vector<HistoryAction> undo_stack;
    std::vector<HistoryAction> redo_stack;
    
    // Cache for Rendering
    std::map<std::string, std::vector<int>, std::greater<std::string>> grouped_user_puzzles;
    bool history_dirty = true;

    void RebuildHistoryCache() {
        if (!history_dirty) return;
        grouped_user_puzzles.clear();
        for (size_t i = 0; i < user_puzzles.size(); ++i) {
            std::string date = user_puzzles[i].first_seen_date;
            if (date.empty()) date = "Unknown Date";
            grouped_user_puzzles[date].push_back(static_cast<int>(i));
        }
        history_dirty = false;
    }
    
    // Theme Filtering
    std::set<std::string> available_themes;
    std::string current_theme_filter = "All";
    std::vector<std::string> multi_theme_filter; // For "Train Weaknesses"

    int current_puzzle_idx = -1;
    size_t current_move_idx = 0;
    Puzzle* current_puzzle = nullptr;

    const std::string HISTORY_FILE = "Assets/user_data/history.json";
    
    // Base Intervals (Leitner fallback / Initial steps)
    const int64_t BASE_INTERVALS[4] = { 600, 86400, 86400*3, 86400*7 };

    PuzzleManager() {
        LoadUserList(HISTORY_FILE, user_puzzles, user_ids);
        RebuildHistoryCache();
    }

    int64_t GetCurrentTimestamp() {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::string GetCurrentDate() {
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm;
        #if defined(_WIN32) || defined(_WIN64)
            localtime_s(&now_tm, &now_c);
        #else
            localtime_r(&now_c, &now_tm);
        #endif
        std::stringstream ss;
        ss << std::put_time(&now_tm, "%Y-%m-%d");
        return ss.str();
    }

    std::string ResolvePath(const std::string& relative_path, bool creating_file = false) {
        if (fs::exists(relative_path)) return relative_path;
        std::string fallback = "../../" + relative_path;
        if (fs::exists(fallback)) return fallback;
        if (creating_file) {
            fs::path p_fallback(fallback);
            if (p_fallback.has_parent_path() && fs::exists(p_fallback.parent_path())) {
                return fallback;
            }
            return relative_path;
        }
        return relative_path; 
    }

    bool LoadPuzzleSet(const std::string& filepath) {
        std::string path = ResolvePath(filepath);
        std::ifstream f(path);
        if (!f.is_open()) return false;
        
        try {
            json data = json::parse(f);
            int added_count = 0;
            available_themes.clear();
            available_themes.insert("All");
            
            active_puzzles.clear(); 
            active_puzzles.shrink_to_fit();
            current_puzzle = nullptr;
            current_puzzle_idx = -1;

            for (const auto& item : data) {
                std::string id = item["PuzzleId"];
                if (user_ids.count(id)) continue; 

                Puzzle p;
                p.fen = item["FEN"];
                p.rating = item["Rating"];
                p.themes = item["Themes"];
                p.id = id;
                p.source_file = path; 
                
                std::string moves_str = item["SANMoves"];
                std::string delimiter = " ";
                size_t pos = 0;
                while ((pos = moves_str.find(delimiter)) != std::string::npos) {
                    p.san_moves.push_back(moves_str.substr(0, pos));
                    moves_str.erase(0, pos + delimiter.length());
                }
                if (!moves_str.empty()) p.san_moves.push_back(moves_str);
                
                std::string themes_str = p.themes;
                pos = 0;
                while ((pos = themes_str.find(delimiter)) != std::string::npos) {
                    available_themes.insert(themes_str.substr(0, pos));
                    themes_str.erase(0, pos + delimiter.length());
                }
                if (!themes_str.empty()) available_themes.insert(themes_str);

                active_puzzles.push_back(p);
                added_count++;
            }
            std::cout << "Loaded " << added_count << " puzzles from " << path << std::endl;
        } catch (...) {
            return false;
        }
        return true;
    }
    
    int LoadDueReviews() {
        active_puzzles.clear();
        int64_t now = GetCurrentTimestamp();
        int count = 0;
        
        for (const auto& p : user_puzzles) {
            bool due = (p.next_review_time > 0 && p.next_review_time <= now);
            bool failed = (p.fails_remaining > 0);
            
            if (due || failed) {
                active_puzzles.push_back(p);
                count++;
            }
        }
        
        std::sort(active_puzzles.begin(), active_puzzles.end(), [](const Puzzle& a, const Puzzle& b) {
            if (a.srs_stage != b.srs_stage) return a.srs_stage < b.srs_stage;
            return a.next_review_time < b.next_review_time;
        });
        
        available_themes.clear();
        available_themes.insert("All");
        
        return count;
    }

    Puzzle* GetRandomPuzzle() {
        if (active_puzzles.empty()) return nullptr;
        
        std::vector<int> valid_indices;
        for(size_t i=0; i<active_puzzles.size(); ++i) {
             bool match = false;
             if (current_theme_filter == "All") match = true;
             else if (active_puzzles[i].themes.find(current_theme_filter) != std::string::npos) match = true;
             
             // Check Multi-Theme Filter (Weaknesses)
             if (!match && !multi_theme_filter.empty()) {
                 for (const auto& t : multi_theme_filter) {
                     if (active_puzzles[i].themes.find(t) != std::string::npos) { match = true; break; }
                 }
             }

             if (match) valid_indices.push_back(static_cast<int>(i));
        }
        
        if (valid_indices.empty()) return nullptr;

        current_puzzle_idx = valid_indices[rand() % valid_indices.size()];
        current_move_idx = 0;
        current_puzzle = &active_puzzles[current_puzzle_idx];
        return current_puzzle;
    }
    
    Puzzle* GetNextReviewPuzzle() {
        if (active_puzzles.empty()) return nullptr;
        current_puzzle_idx = 0;
        current_move_idx = 0;
        current_puzzle = &active_puzzles[0];
        return current_puzzle;
    }

    Puzzle* LoadPuzzleById(const std::string& id) {
        // 1. Check currently loaded chunks first for a fast return
        for (auto& pair : loaded_chunks) {
            for (auto& p : pair.second) {
                if (p.id == id) {
                    active_puzzles.clear();
                    active_puzzles.push_back(p);
                    current_puzzle_idx = 0;
                    current_move_idx = 0;
                    current_puzzle = &active_puzzles[0];
                    return current_puzzle;
                }
            }
        }

        // 2. Scan all puzzle files in Assets/data/
        std::string data_dir = ResolvePath("Assets/data");
        if (!fs::exists(data_dir)) return nullptr;

        for (const auto& entry : fs::directory_iterator(data_dir)) {
            std::string path = entry.path().string();
            if (path.find("puzzles_") != std::string::npos && path.find(".json") != std::string::npos) {
                std::ifstream f(path);
                if (f.is_open()) {
                    try {
                        json data = json::parse(f);
                        for (const auto& item : data) {
                            if (item["PuzzleId"] == id) {
                                Puzzle p;
                                p.fen = item["FEN"];
                                p.rating = item["Rating"];
                                p.themes = item["Themes"];
                                p.id = id;
                                p.source_file = path;
                                
                                std::string moves_str = item["SANMoves"];
                                std::string delimiter = " ";
                                size_t pos = 0;
                                while ((pos = moves_str.find(delimiter)) != std::string::npos) {
                                    p.san_moves.push_back(moves_str.substr(0, pos));
                                    moves_str.erase(0, pos + delimiter.length());
                                }
                                if (!moves_str.empty()) p.san_moves.push_back(moves_str);
                                
                                active_puzzles.clear();
                                active_puzzles.push_back(p);
                                current_puzzle_idx = 0;
                                current_move_idx = 0;
                                current_puzzle = &active_puzzles[0];
                                return current_puzzle;
                            }
                        }
                    } catch (...) {}
                }
            }
        }
        return nullptr;
    }

    Puzzle* GetProgressivePuzzle(int current_score) {
        int target_rating = 500 + current_score * 20;
        if (target_rating > 3000) target_rating = 3000;

        if (!EnsureChunkLoaded(target_rating)) {
            return GetRandomPuzzle(); // Extreme fallback if no files exist at all
        }
        
        int bucket = current_chunk_bucket;
        if (loaded_chunks.find(bucket) == loaded_chunks.end() || loaded_chunks[bucket].empty()) {
            return GetRandomPuzzle();
        }

        std::vector<int> valid_indices;
        int best_diff = 10000;

        const auto& chunk = loaded_chunks[bucket];
        for (size_t i = 0; i < chunk.size(); ++i) {
            // Prevent duplicates in same session
            if (session_used_ids.count(chunk[i].id)) continue;

            bool match = false;
            if (current_theme_filter == "All") match = true;
            else if (chunk[i].themes.find(current_theme_filter) != std::string::npos) match = true;

            if (!match && !multi_theme_filter.empty()) {
                for (const auto& t : multi_theme_filter) {
                    if (chunk[i].themes.find(t) != std::string::npos) { match = true; break; }
                }
            }
            
            if (!match) continue;

            int puzzle_rating = 1500;
            try { puzzle_rating = std::stoi(chunk[i].rating); } catch (...) {}
            
            int diff = std::abs(puzzle_rating - target_rating);
            if (diff < best_diff) {
                best_diff = diff;
                valid_indices.clear();
                valid_indices.push_back(static_cast<int>(i));
            } else if (diff == best_diff) { 
                valid_indices.push_back(static_cast<int>(i));
            }
        }

        if (valid_indices.empty()) {
            // If the filtered bucket is empty, we fall back to active_puzzles (free play loaded set) 
            // or we could force it to scan higher buckets. For now, random fallback is safe.
            return GetRandomPuzzle(); 
        }

        int selected_idx = valid_indices[rand() % valid_indices.size()];
        
        // We must push the selected puzzle into active_puzzles so the rest of the 
        // app (which expects current_puzzle to be inside active_puzzles) functions correctly.
        active_puzzles.clear(); // Ensure only 1 active puzzle during progressive mode to save RAM
        active_puzzles.push_back(chunk[selected_idx]);
        
        session_used_ids.insert(chunk[selected_idx].id);
        
        current_puzzle_idx = 0;
        current_move_idx = 0;
        current_puzzle = &active_puzzles[0];
        return current_puzzle;
    }
    
    double MarkCurrentSolved(double solve_time_seconds) {
        if (!current_puzzle) return 0.0;
        
        current_puzzle->completion_date = GetCurrentDate();
        if (current_puzzle->first_seen_date.empty()) current_puzzle->first_seen_date = current_puzzle->completion_date;

        current_puzzle->total_seconds_spent += solve_time_seconds;
        
        if (current_puzzle->fails_remaining == 0) {
            if (solve_time_seconds < 10.0) current_puzzle->ease_factor += 0.15;
            else if (solve_time_seconds > 30.0) current_puzzle->ease_factor -= 0.15;
        }
        if (current_puzzle->ease_factor < 1.3) current_puzzle->ease_factor = 1.3;

        current_puzzle->srs_stage++;
        
        int64_t interval = 0;
        if (current_puzzle->srs_stage < 4) {
            int idx = std::min(current_puzzle->srs_stage, 3);
            interval = BASE_INTERVALS[idx];
        } else {
            if (current_puzzle->last_interval == 0) current_puzzle->last_interval = BASE_INTERVALS[3]; 
            interval = (int64_t)(current_puzzle->last_interval * current_puzzle->ease_factor);
        }
        
        current_puzzle->last_interval = interval;
        current_puzzle->next_review_time = GetCurrentTimestamp() + interval;

        auto it = std::find_if(user_puzzles.begin(), user_puzzles.end(), [&](const Puzzle& p) {
            return p.id == current_puzzle->id;
        });

        if (it != user_puzzles.end()) {
            if (it->fails_remaining > 0) it->fails_remaining--;
            it->srs_stage = current_puzzle->srs_stage;
            it->next_review_time = current_puzzle->next_review_time;
            it->completion_date = current_puzzle->completion_date;
            it->ease_factor = current_puzzle->ease_factor;
            it->last_interval = current_puzzle->last_interval;
            it->total_seconds_spent = current_puzzle->total_seconds_spent;
        } else {
            current_puzzle->fails_remaining = 0;
            user_puzzles.push_back(*current_puzzle);
            user_ids.insert(current_puzzle->id);
        }
        
        double final_total_time = current_puzzle->total_seconds_spent;
        SaveUserList(HISTORY_FILE, user_puzzles);
        history_dirty = true;
        
        active_puzzles.erase(active_puzzles.begin() + current_puzzle_idx);
        current_puzzle = nullptr;
        current_puzzle_idx = -1;
        
        return final_total_time;
    }

    void MarkCurrentFailed(bool keep_active = false, double time_spent = 0.0) {
        if (!current_puzzle) return;
        current_puzzle->completion_date = GetCurrentDate();
        if (current_puzzle->first_seen_date.empty()) current_puzzle->first_seen_date = current_puzzle->completion_date;

        current_puzzle->total_seconds_spent += time_spent;

        if (current_puzzle->srs_stage > 1) current_puzzle->srs_stage -= 1;
        else current_puzzle->srs_stage = 0;
        
        current_puzzle->ease_factor -= 0.2;
        if (current_puzzle->ease_factor < 1.3) current_puzzle->ease_factor = 1.3;

        current_puzzle->next_review_time = GetCurrentTimestamp(); 
        current_puzzle->last_interval = 0; 

        auto it = std::find_if(user_puzzles.begin(), user_puzzles.end(), [&](const Puzzle& p) {
            return p.id == current_puzzle->id;
        });

        if (it != user_puzzles.end()) {
            it->fails_remaining = 3; 
            it->completion_date = current_puzzle->completion_date;
            it->srs_stage = current_puzzle->srs_stage;
            it->next_review_time = current_puzzle->next_review_time;
            it->ease_factor = current_puzzle->ease_factor;
            it->last_interval = 0;
            it->total_seconds_spent = current_puzzle->total_seconds_spent;
        } else {
            current_puzzle->fails_remaining = 3;
            user_puzzles.push_back(*current_puzzle);
            user_ids.insert(current_puzzle->id);
        }
        
        SaveUserList(HISTORY_FILE, user_puzzles);
        history_dirty = true;

        if (!keep_active) {
            active_puzzles.erase(active_puzzles.begin() + current_puzzle_idx);
            current_puzzle = nullptr;
            current_puzzle_idx = -1;
        }
    }
    
    void ReplayPuzzle(int index) {
        if (index < 0 || index >= (int)user_puzzles.size()) return;
        active_puzzles.push_back(user_puzzles[index]);
        current_puzzle_idx = static_cast<int>(active_puzzles.size()) - 1;
        current_puzzle = &active_puzzles.back();
        current_move_idx = 0;
    }

    Puzzle* GetCurrentPuzzle() {
        return current_puzzle;
    }
    
    // --- DELETE / UNDO / REDO ---
    
    void DeletePuzzle(const std::string& id) {
        auto it = std::find_if(user_puzzles.begin(), user_puzzles.end(), [&](const Puzzle& p) { return p.id == id; });
        if (it != user_puzzles.end()) {
            // Push to undo stack
            int index = (int)std::distance(user_puzzles.begin(), it);
            undo_stack.push_back({*it, index});
            redo_stack.clear(); 
            
            user_ids.erase(id);
            user_puzzles.erase(it);
            SaveUserList(HISTORY_FILE, user_puzzles);
            history_dirty = true;
        }
    }
    
    void UndoDelete() {
        if (undo_stack.empty()) return;
        
        HistoryAction action = undo_stack.back();
        undo_stack.pop_back();
        
        // Restore
        if (action.original_index >= 0 && action.original_index <= (int)user_puzzles.size()) {
            user_puzzles.insert(user_puzzles.begin() + action.original_index, action.p);
        } else {
            user_puzzles.push_back(action.p);
        }
        user_ids.insert(action.p.id);
        
        // Add to redo
        redo_stack.push_back(action);
        SaveUserList(HISTORY_FILE, user_puzzles);
        history_dirty = true;
    }
    
    void RedoDelete() {
        if (redo_stack.empty()) return;
        
        HistoryAction action = redo_stack.back();
        redo_stack.pop_back();
        
        // Re-Delete
        // We assume ID is unique
        auto it = std::find_if(user_puzzles.begin(), user_puzzles.end(), [&](const Puzzle& p) { return p.id == action.p.id; });
        if (it != user_puzzles.end()) {
            user_ids.erase(action.p.id);
            user_puzzles.erase(it);
            
            // Add back to undo
            undo_stack.push_back(action);
            SaveUserList(HISTORY_FILE, user_puzzles);
            history_dirty = true;
        }
    }
    
    std::map<std::string, int> GetStats() {
        std::map<std::string, int> stats;
        for (const auto& p : user_puzzles) {
            // We use completion_date for "Activity"
            if (!p.completion_date.empty()) {
                stats[p.completion_date]++;
            }
        }
        return stats;
    }

private:
    bool EnsureChunkLoaded(int target_rating) {
        int requested_bucket = (target_rating / 100) * 100;
        
        int bucket = requested_bucket;
        std::string path;
        while (bucket <= 4000) { 
            path = ResolvePath("Assets/data/puzzles_" + std::to_string(bucket) + ".json");
            if (std::filesystem::exists(path)) break;
            bucket += 100;
        }
        
        if (bucket <= 4000 && loaded_chunks.find(bucket) == loaded_chunks.end()) {
            LoadPuzzleSetToChunk(path, bucket);
            
            std::vector<int> to_erase;
            for (const auto& pair : loaded_chunks) {
                // Keep only the chunk before, current, and two chunks ahead to be safe
                if (pair.first < bucket - 100 || pair.first > bucket + 200) {
                    to_erase.push_back(pair.first);
                }
            }
            for (int k : to_erase) loaded_chunks.erase(k);
            
            current_chunk_bucket = bucket;
            return true;
        }
        
        if (bucket <= 4000) current_chunk_bucket = bucket;
        return loaded_chunks.find(bucket) != loaded_chunks.end();
    }

    void LoadPuzzleSetToChunk(const std::string& path, int bucket) {
        std::ifstream f(path);
        if (!f.is_open()) return;
        
        try {
            json data = json::parse(f);
            std::vector<Puzzle> chunk_puzzles;
            
            // Rebuild themes
            available_themes.clear();
            available_themes.insert("All");

            for (const auto& item : data) {
                std::string id = item["PuzzleId"];
                if (user_ids.count(id)) continue; 

                Puzzle p;
                p.fen = item["FEN"];
                p.rating = item["Rating"];
                p.themes = item["Themes"];
                p.id = id;
                p.source_file = path; 
                
                std::string moves_str = item["SANMoves"];
                std::string delimiter = " ";
                size_t pos = 0;
                while ((pos = moves_str.find(delimiter)) != std::string::npos) {
                    p.san_moves.push_back(moves_str.substr(0, pos));
                    moves_str.erase(0, pos + delimiter.length());
                }
                if (!moves_str.empty()) p.san_moves.push_back(moves_str);
                
                std::string themes_str = p.themes;
                pos = 0;
                while ((pos = themes_str.find(delimiter)) != std::string::npos) {
                    available_themes.insert(themes_str.substr(0, pos));
                    themes_str.erase(0, pos + delimiter.length());
                }
                if (!themes_str.empty()) available_themes.insert(themes_str);

                chunk_puzzles.push_back(p);
            }
            loaded_chunks[bucket] = chunk_puzzles;
            std::cout << "Dynamically loaded " << chunk_puzzles.size() << " puzzles into bucket " << bucket << std::endl;
        } catch (...) {
            std::cout << "Failed to parse chunk: " << path << std::endl;
        }
    }

    void LoadUserList(const std::string& filepath, std::vector<Puzzle>& list, std::unordered_set<std::string>& ids) {
        std::string path = ResolvePath(filepath);
        std::ifstream f(path);
        if (!f.is_open()) return;
        
        try {
            json data = json::parse(f);
            for (const auto& item : data) {
                Puzzle p;
                p.id = item["PuzzleId"];
                p.fen = item["FEN"];
                p.rating = item["Rating"];
                p.san_moves = item["SANMoves"].get<std::vector<std::string>>();
                
                if (item.contains("Date")) p.completion_date = item["Date"];
                else p.completion_date = "Unknown Date";
                
                if (item.contains("FirstSeen")) p.first_seen_date = item["FirstSeen"];
                else p.first_seen_date = p.completion_date;

                if (item.contains("SRS_Stage")) p.srs_stage = item["SRS_Stage"];
                if (item.contains("NextReview")) p.next_review_time = item["NextReview"];
                
                if (item.contains("Fails")) p.fails_remaining = item["Fails"];
                else p.fails_remaining = 0;
                
                if (item.contains("Ease")) p.ease_factor = item["Ease"];
                else p.ease_factor = 2.5;
                
                if (item.contains("LastInt")) p.last_interval = item["LastInt"];
                else p.last_interval = 0;
                
                if (item.contains("TotalTime")) p.total_seconds_spent = item["TotalTime"];
                else p.total_seconds_spent = 0.0;

                list.push_back(p);
                ids.insert(p.id);
            }
        } catch (...) {}
    }

    void SaveUserList(const std::string& relative_path, const std::vector<Puzzle>& list) {
        std::string path = ResolvePath(relative_path, true);
        fs::path p(path);
        if (p.has_parent_path()) {
            try { if (!fs::exists(p.parent_path())) fs::create_directories(p.parent_path()); } catch (...) {}
        }

        json j_list = json::array();
        for (const auto& item : list) {
            json j;
            j["PuzzleId"] = item.id;
            j["FEN"] = item.fen;
            j["Rating"] = item.rating;
            j["SANMoves"] = item.san_moves;
            j["Date"] = item.completion_date;
            j["FirstSeen"] = item.first_seen_date;
            j["SRS_Stage"] = item.srs_stage;
            j["NextReview"] = item.next_review_time;
            j["Fails"] = item.fails_remaining;
            j["Ease"] = item.ease_factor;
            j["LastInt"] = item.last_interval;
            j["TotalTime"] = item.total_seconds_spent;
            j_list.push_back(j);
        }
        
        std::thread([path, j_list]() {
            std::ofstream f(path);
            if (f.is_open()) f << j_list.dump(); 
        }).detach(); 
    }
};
