#pragma once
#include <string>
#include <vector>
#include <thread>
#include <map>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <nlohmann/json.hpp>
#include <ctime>
#include <algorithm>
#include <cmath>
#include <regex>
#include <filesystem>

struct DailyPerformance {
    int puzzles_attempted = 0;
    int puzzles_solved = 0;
    float total_time_spent = 0.0f;
    float total_tti = 0.0f; // Total Time-to-Insight
    
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(DailyPerformance, 
        puzzles_attempted, puzzles_solved, total_time_spent, total_tti
    )
};

struct ThemeProficiency {
    int attempts = 0;
    int successes = 0;
    float rating = 1200.0f; 
    float rd = 350.0f;        // Rating Deviation
    float volatility = 0.06f; 
    std::vector<int> recent_results; // Store last 20 results (1=win, 0=loss)

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ThemeProficiency, attempts, successes, rating, rd, volatility, recent_results)
};

struct ArcadeRecords {
    int best_storm_score = 0;
    int best_storm_combo = 0;
    int best_streak_score = 0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ArcadeRecords, best_storm_score, best_storm_combo, best_streak_score)
};

struct StatsState {
    std::map<std::string, DailyPerformance> daily_history; 
    std::map<std::string, ThemeProficiency> theme_proficiencies; 
    std::vector<float> tiq_history; 
    float current_tiq = 1200.0f;    
    ArcadeRecords arcade_records;
    
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(StatsState, 
        daily_history, theme_proficiencies, tiq_history, current_tiq, arcade_records
    )
};

class StatsManager {
public:
    StatsState state;
    const float TAU = 0.5f; // System Constant for Glicko-2 (Volatile)

    bool ValidateState(const StatsState& s) {
        if (s.current_tiq < 100.0f || s.current_tiq > 4000.0f) return false;
        return true; 
    }

    void Load(const std::string& path) {
        namespace fs = std::filesystem;
        std::vector<std::string> paths = { path, path + ".bak", path + ".old" };
        
        for (const auto& p : paths) {
            if (!fs::exists(p)) continue;
            
            std::ifstream f(p);
            if (f.is_open()) {
                try {
                    nlohmann::json j;
                    f >> j;
                    StatsState loaded_state = j.get<StatsState>();
                    if (ValidateState(loaded_state)) {
                        state = loaded_state;
                        if (p != path) Save(path); // Repair primary
                        return;
                    }
                } catch (...) { }
            }
        }
    }

    void Save(const std::string& path) {
        namespace fs = std::filesystem;
        try {
            // Rotate Backups
            if (fs::exists(path + ".bak")) {
                if (fs::exists(path + ".old")) fs::remove(path + ".old");
                fs::rename(path + ".bak", path + ".old");
            }
            if (fs::exists(path)) {
                fs::rename(path, path + ".bak");
            }

            // Atomic Write
            std::string tmp_path = path + ".tmp";
            {
                nlohmann::json j = state;
                std::ofstream f(tmp_path);
                if (f.is_open()) {
                    f << std::setw(4) << j << std::endl;
                } else return;
            }

            if (fs::exists(tmp_path) && fs::file_size(tmp_path) > 0) {
                fs::rename(tmp_path, path);
            }
        } catch (...) { }
    }

    bool UpdateArcadeRecords(int storm_score, int storm_combo, int streak_score) {
        bool updated = false;
        if (storm_score > state.arcade_records.best_storm_score) {
            state.arcade_records.best_storm_score = storm_score;
            updated = true;
        }
        if (storm_combo > state.arcade_records.best_storm_combo) {
            state.arcade_records.best_storm_combo = storm_combo;
            updated = true;
        }
        if (streak_score > state.arcade_records.best_streak_score) {
            state.arcade_records.best_streak_score = streak_score;
            updated = true;
        }
        return updated;
    }

    std::string GetCurrentDate() {
        std::time_t t = std::time(nullptr);
        std::tm now_tm;
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&now_tm, &t);
#else
        localtime_r(&t, &now_tm);
#endif
        char buf[11];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d", &now_tm);
        return std::string(buf);
    }

    // Glicko-2 Simplified Helper Functions
    float g(float rd) {
        return 1.0f / sqrtf(1.0f + 3.0f * powf(rd / 400.0f * 0.5513f, 2.0f));
    }

    float E(float r, float r_j, float rd_j) {
        return 1.0f / (1.0f + powf(10.0f, -g(rd_j) * (r - r_j) / 400.0f));
    }

    void RecordPuzzleResult(bool success, float time_spent, float tti, const std::string& themes_str, const std::string& puzzle_rating_str) {
        std::string today = GetCurrentDate();
        auto& daily = state.daily_history[today];
        
        daily.puzzles_attempted++;
        if (success) daily.puzzles_solved++;
        daily.total_time_spent += time_spent;
        daily.total_tti += tti;

        float p_rating = 1500.0f;
        try { p_rating = std::stof(puzzle_rating_str); } catch (...) {}

        std::regex re("[,\\s]+");
        std::sregex_token_iterator it(themes_str.begin(), themes_str.end(), re, -1);
        std::sregex_token_iterator reg_end;

        for (; it != reg_end; ++it) {
            std::string theme = it->str();
            if (theme.length() < 2) continue;
            
            auto& tp = state.theme_proficiencies[theme];
            tp.attempts++;
            if (success) tp.successes++;
            
            float score = success ? 1.0f : 0.0f;
            float opp_rd = 50.0f;
            float g_val = g(opp_rd);
            float expected = E(tp.rating, p_rating, opp_rd);
            
            if (tp.rd < 350.0f) tp.rd = sqrtf(powf(tp.rd, 2.0f) + 150.0f);
            
            float K = 800.0f / (tp.attempts + 10.0f);
            if (K < 16.0f) K = 16.0f;
            tp.rating += K * (score - expected);
            tp.rd = std::max(30.0f, tp.rd * 0.98f); 

            tp.recent_results.push_back(success ? 1 : 0);
            if (tp.recent_results.size() > 20) tp.recent_results.erase(tp.recent_results.begin());
        }

        float k_factor = 20.0f;
        float expected_tiq = 1.0f / (1.0f + powf(10.0f, (p_rating - state.current_tiq) / 400.0f)); 
        float actual_tiq = success ? 1.0f : 0.0f;
        
        float base_change = k_factor * (actual_tiq - expected_tiq);
        if (success) {
            float speed_mult = 1.0f + std::max(-0.2f, (15.0f - tti) / 60.0f);
            base_change *= speed_mult;
        }

        state.current_tiq += base_change;
        
        if ((daily.puzzles_attempted % 5) == 0) {
            state.tiq_history.push_back(state.current_tiq);
            if (state.tiq_history.size() > 100) state.tiq_history.erase(state.tiq_history.begin());
        }
    }

    float GetDailyDPI(const std::string& date) {
        if (state.daily_history.find(date) == state.daily_history.end()) return 0.0f;
        const auto& d = state.daily_history.at(date);
        if (d.puzzles_attempted == 0) return 0.0f;

        float accuracy = (float)d.puzzles_solved / (float)d.puzzles_attempted;
        float avg_tti = d.total_tti / (float)d.puzzles_attempted;
        float speed_factor = std::clamp(1.0f - (avg_tti / 45.0f), 0.0f, 1.0f);

        return (accuracy * 70.0f) + (speed_factor * 30.0f);
    }

    std::vector<std::string> GetWeakestThemes(int count = 3) {
        std::vector<std::pair<std::string, float>> themes;
        for (auto const& [name, prof] : state.theme_proficiencies) {
            if (prof.attempts < 5) continue;
            themes.push_back({name, prof.rating});
        }
        std::sort(themes.begin(), themes.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
        
        std::vector<std::string> result;
        for (int i = 0; i < std::min((int)themes.size(), count); ++i) result.push_back(themes[i].first);
        return result;
    }

    std::map<std::string, float> GetDimensionRatings() {
        std::map<std::string, float> dimensions;
        std::map<std::string, int> counts;
        std::vector<std::string> dims = { "Tactics", "Checkmate", "Advanced", "Endgame", "Opening/Def" };
        for (const auto& d : dims) { dimensions[d] = 0.0f; counts[d] = 0; }

        auto AddToDim = [&](const std::string& dim, float rating) {
            dimensions[dim] += rating;
            counts[dim]++;
        };

        for (auto const& [theme, prof] : state.theme_proficiencies) {
            std::string t = theme;
            std::transform(t.begin(), t.end(), t.begin(), ::tolower);

            if (t.find("fork") != std::string::npos || t.find("pin") != std::string::npos || t.find("skewer") != std::string::npos || 
                t.find("discovered") != std::string::npos || t.find("hanging") != std::string::npos) {
                AddToDim("Tactics", prof.rating);
            }
            else if (t.find("mate") != std::string::npos || t.find("backrank") != std::string::npos || t.find("smothered") != std::string::npos) {
                AddToDim("Checkmate", prof.rating);
            }
            else if (t.find("attraction") != std::string::npos || t.find("deflection") != std::string::npos || t.find("intermezzo") != std::string::npos || 
                     t.find("zugzwang") != std::string::npos || t.find("quiet") != std::string::npos) {
                AddToDim("Advanced", prof.rating);
            }
            else if (t.find("endgame") != std::string::npos || t.find("promotion") != std::string::npos) {
                AddToDim("Endgame", prof.rating);
            }
            else if (t.find("opening") != std::string::npos || t.find("enpassant") != std::string::npos || t.find("castling") != std::string::npos || 
                     t.find("defensive") != std::string::npos || t.find("stalemate") != std::string::npos || t.find("trapped") != std::string::npos) {
                AddToDim("Opening/Def", prof.rating);
            }
        }

        for (const auto& d : dims) {
            if (counts[d] > 0) dimensions[d] /= counts[d];
            else dimensions[d] = 1200.0f;
        }
        return dimensions;
    }

    struct AnnualHighlights {
        int longest_streak = 0;
        int max_solved_day = 0;
        std::string max_day_date = "N/A";
        float avg_accuracy = 0.0f;
        int total_attempted = 0;
        int total_solved = 0;
    };

    AnnualHighlights GetAnnualHighlights(int year) {
        AnnualHighlights h;
        std::string year_prefix = std::to_string(year) + "-";
        std::vector<std::string> active_dates;
        for (auto const& [date, perf] : state.daily_history) {
            if (date.find(year_prefix) == 0 && perf.puzzles_solved > 0) {
                active_dates.push_back(date);
                h.total_attempted += perf.puzzles_attempted;
                h.total_solved += perf.puzzles_solved;
                if (perf.puzzles_solved > h.max_solved_day) {
                    h.max_solved_day = perf.puzzles_solved;
                    h.max_day_date = date;
                }
            }
        }
        if (h.total_attempted > 0) h.avg_accuracy = (float)h.total_solved / h.total_attempted * 100.0f;
        if (!active_dates.empty()) {
            std::sort(active_dates.begin(), active_dates.end());
            int current_streak = 1; h.longest_streak = 1;
            for (size_t i = 1; i < active_dates.size(); ++i) {
                std::tm tm1 = {0}, tm2 = {0};
                #if defined(_WIN32) || defined(_WIN64)
                sscanf_s(active_dates[i-1].c_str(), "%d-%d-%d", &tm1.tm_year, &tm1.tm_mon, &tm1.tm_mday);
                sscanf_s(active_dates[i].c_str(), "%d-%d-%d", &tm2.tm_year, &tm2.tm_mon, &tm2.tm_mday);
                #else
                sscanf(active_dates[i-1].c_str(), "%d-%d-%d", &tm1.tm_year, &tm1.tm_mon, &tm1.tm_mday);
                sscanf(active_dates[i].c_str(), "%d-%d-%d", &tm2.tm_year, &tm2.tm_mon, &tm2.tm_mday);
                #endif
                tm1.tm_year -= 1900; tm1.tm_mon -= 1; tm2.tm_year -= 1900; tm2.tm_mon -= 1;
                std::time_t t1 = std::mktime(&tm1); std::time_t t2 = std::mktime(&tm2);
                if (std::difftime(t2, t1) <= 90000.0) {
                    current_streak++;
                    if (current_streak > h.longest_streak) h.longest_streak = current_streak;
                } else { current_streak = 1; }
            }
        }
        return h;
    }

    std::string GetStateOfMind() {
        if (state.daily_history.empty()) return "Welcome! Solve some puzzles to generate your tactical profile.";
        auto it = state.daily_history.rbegin();
        const auto& today = it->second;
        if (today.puzzles_attempted < 3) return "Keep going! I need a few more puzzles to analyze your current form.";
        float accuracy = (float)today.puzzles_solved / today.puzzles_attempted;
        float avg_tti = today.total_tti / today.puzzles_attempted;
        std::string summary = "";
        if (accuracy > 0.85f) summary += "Your tactical vision is razor sharp today. ";
        else if (accuracy < 0.5f) summary += "You seem to be struggling with calculation. ";
        else summary += "Your performance is steady. ";
        if (avg_tti < 8.0f && accuracy < 0.7f) summary += "You are moving too fast! Slow down and verify your lines.";
        else if (avg_tti > 25.0f && accuracy > 0.8f) summary += "Your deep calculation is paying off, though you are taking your time.";
        else if (avg_tti < 12.0f && accuracy > 0.85f) summary += "Your intuition is incredible; you're seeing the solutions almost instantly.";
        auto weak = GetWeakestThemes(1);
        if (!weak.empty()) summary += "\n\nTargeted Advice: Your rating in '" + weak[0] + "' is lagging. Focus on those patterns.";
        return summary;
    }

    float GetMaxTIQ() {
        if (state.tiq_history.empty()) return state.current_tiq;
        float max_v = state.current_tiq;
        for (float v : state.tiq_history) if (v > max_v) max_v = v;
        return max_v;
    }

    struct RPGStats {
        int agility; int wisdom; int tenacity; int power;
    };

    RPGStats GetRPGStats() {
        RPGStats s = {0,0,0,0};
        if (state.daily_history.empty()) return s;
        long total_solved = 0, total_attempted = 0;
        for (const auto& [date, day] : state.daily_history) {
            total_solved += day.puzzles_solved;
            total_attempted += day.puzzles_attempted;
        }
        if (total_attempted > 0) s.wisdom = (int)((float)total_solved / total_attempted * 100.0f);
        int days_checked = 0; float total_tti = 0; int puzzles_for_tti = 0;
        for (auto it = state.daily_history.rbegin(); it != state.daily_history.rend(); ++it) {
            if (it->second.puzzles_attempted > 0) {
                total_tti += it->second.total_tti;
                puzzles_for_tti += it->second.puzzles_attempted;
                days_checked++; if (days_checked >= 5) break;
            }
        }
        if (puzzles_for_tti > 0) {
            float avg_tti = total_tti / puzzles_for_tti;
            s.agility = std::clamp((int)((30.0f - avg_tti) / 25.0f * 100.0f), 0, 100);
        }
        s.tenacity = std::clamp((int)(state.daily_history.size() * 3.3f), 0, 100);
        s.power = std::clamp((int)((state.current_tiq - 1200.0f) / 12.0f), 0, 100);
        return s;
    }
};
