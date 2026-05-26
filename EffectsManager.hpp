#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include "chess.hpp"

class EffectsManager {
public:
    enum class EffectType { FIREWORK, SPARK };
    enum class TrailStyle { SPARKS, CYBER, ICE, WATER, FIRE };
    enum class TrailMode { ECHO, LIGHT_STREAK, PARTICLES, BLUR };
    enum class SparkStyle { CLASSIC, MAGICAL, RADIAL, VORTEX };
    enum class FireworkStyle { RINGS, RED_SHELL, GOLDEN_WILLOW };
    
    struct PendingEffect { EffectType type; chess::Square sq; ImU32 color; };

    struct Star {
        ImVec2 pos;
        float speed;
        float size;
        int layer; // 0=BG, 1=Mid, 2=FG
        ImU32 base_color; // Intrinsic star classification color
    };

    struct ShootingStar {
        ImVec2 pos;
        ImVec2 vel;
        float life;
        float max_life;
    };

    struct Particle {
        ImVec2 pos = {0.0f, 0.0f};
        ImVec2 vel = {0.0f, 0.0f};
        ImU32 color = IM_COL32(255, 255, 255, 255);
        float life = 1.0f; 
        float max_life = 1.0f;
        float size = 5.0f;
        float initial_size = 5.0f;
        bool has_gravity = true;
        float gravity_strength = 600.0f;
        float air_resistance = 0.98f;
        bool is_square = false;
        bool is_diamond = false;
        int ember_style = -1;
        bool is_firework = false;
        
        ImVec2 history[10] = { {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0} };
        bool has_history = false;
    };
    
    struct Ghost { ImVec2 pos; int piece_idx; float life; float max_life; ImVec2 velocity = {0,0}; };
    struct TrailPoint { ImVec2 pos; float life; float max_life; };
    struct Shockwave { ImVec2 pos; float life; float max_life; ImU32 color; float initial_size; float max_size; };
    struct LandingFlash { ImVec2 pos; float life; float max_life; ImU32 color; };
    struct PieceSquash { chess::Square sq; float timer; float intensity; };
    struct Ripple { ImVec2 pos; float life; float max_life; float size; };
    struct ImpactBoom { float life; float max_life; float intensity; };
    struct ImpactWobble { chess::Square center; float life; float max_life; float radius; float intensity; bool is_white_enemy; bool is_correct_move; };

    enum class CelestialType { PLANET, GAS_GIANT, NEBULA, BLACK_HOLE };
    struct CelestialBody {
        ImVec2 pos;
        float radius;
        CelestialType type;
        ImU32 base_color;
        float distance_scale;
        float life;
    };

    const float DEFAULT_GRAVITY = 600.0f;
    const float DEFAULT_AIR_RESISTANCE = 0.98f;
    const int MAX_PARTICLES = 3000;

    // Settings Flags
    bool disable_all_effects = false;
    bool enable_shake = true;
    bool enable_fireworks = true;
    bool enable_jiggle = true;
    bool enable_ghost_move = true;
    bool enable_pulse = true;
    bool enable_trail = true;
    bool enable_wobble = true;
    bool enable_rotation = true;
    bool enable_combo_heat = true;
    bool enable_thud = true;
    bool enable_physicality = true;
    bool enable_living_check = true;
    bool enable_victory_dance = true;
    bool enable_square_colors = false;

    // Sub-settings
    float phys_bounce_strength = 1.0f;
    float phys_shake_intensity = 1.0f;
    float phys_tilt_amount = 1.0f;
    int victory_dance_type = 0;
    float victory_dance_intensity = 1.0f;
    float victory_dance_duration = 3.0f;
    float thud_global_intensity = 1.0f;
    float thud_squash_factor = 1.0f;
    float thud_flash_brightness = 1.0f;
    float thud_puff_density = 1.0f;
    float thud_ripple_scale = 1.0f;
    
    float board_tint_r = 1.0f;
    float board_tint_g = 1.0f;
    float board_tint_b = 1.0f;
    float board_hue_shift = 0.0f;
    float board_brightness = 1.0f;
    float edge_color_r = 0.15f;
    float edge_color_g = 0.13f;
    float edge_color_b = 0.11f;

    TrailMode current_trail_mode = TrailMode::ECHO;
    float trail_life = 0.4f;
    int trail_blur_density = 8;
    float trail_streak_thickness = 4.0f;

    float heat_intensity = 1.0f;
    float combo_shake_intensity = 1.0f;
    SparkStyle current_spark_style = SparkStyle::CLASSIC;
    bool enable_starfield = true;
    ImVec2 starfield_origin = {0,0};
    float starfield_speed = 1.0f;
    float starfield_trail_size = 1.0f;

    int fw_particle_count = 60;
    float fw_explosion_power = 1.0f; 
    float fw_gravity_scale = 1.0f;   
    float fw_particle_size = 4.0f;
    int fw_count = 1;
    float fw_particle_speed = 1.0f;
    float fw_duration_scale = 1.0f;
    FireworkStyle current_firework_style = FireworkStyle::RINGS;
    
    float trail_speed_scale = 1.0f;
    float trail_size_scale = 1.0f;
    int trail_particle_count = 6;
    TrailStyle current_trail_style = TrailStyle::SPARKS;
    
    // Per-Square Effects Toggles
    bool enable_hover_elevation = true;
    bool enable_domino_wave = true;
    float domino_wave_intensity = 1.0f;
    float domino_wave_range = 14.0f;
    float domino_wave_speed = 25.0f;
    float domino_wave_bounce = 1.0f;
    float domino_wave_glow = 1.0f;
    float domino_wave_wobble = 1.0f;
    float domino_wave_duration = 0.4f;
    
    // Idle Settings
    float idle_delay_seconds = 5.0f;
    int idle_fps_cap = 1;

    friend void to_json(nlohmann::json& j, const EffectsManager& t);
    friend void from_json(const nlohmann::json& j, EffectsManager& t);
    struct FireworkInstance { ImVec2 center; ImU32 color; float delay; };

    // State
    std::vector<FireworkInstance> active_firework_instances;
    std::vector<Star> stars;
    std::vector<ShootingStar> shooting_stars;
    std::vector<CelestialBody> celestial_bodies;
    int last_celestial_combo = 0;
    std::vector<Particle> particles;
    std::vector<Ghost> ghosts;
    std::vector<TrailPoint> trail_points;
    std::vector<Shockwave> shockwaves;
    std::vector<LandingFlash> flashes;
    std::vector<PieceSquash> squashes;
    std::vector<Ripple> ripples;
    std::vector<ImpactWobble> impact_wobbles;
    std::vector<PendingEffect> pending_effects;
    
    double trauma = 0.0;
    double jiggle_timer = 0.0;
    const double JIGGLE_DURATION = 0.4;
    double wobble_timer = 0.0;
    double victory_timer = 0.0;
    float current_dynamic_starfield_speed = -1.0f;
    
    // Per-Square Effects State
    float hover_elevations[64] = {0.0f};
    chess::Square current_hovered_sq = chess::Square::underlying::NO_SQ;

    // --- Pre-calculated Frame Math ---
    float cached_depression[64] = {0.0f};
    ImVec2 cached_tilt[64] = {};
    float cached_wobble_x[64] = {0.0f};
    float cached_wobble_y[64] = {0.0f};
    float cached_enemy_shake_x[2][64] = {{0.0f}};
    float cached_enemy_shake_y[2][64] = {{0.0f}};
    float cached_jiggle[64] = {0.0f};

    void Precalculate(bool player_is_white) {
        if (disable_all_effects) return;
        for (int i = 0; i < 64; ++i) {
            chess::Square sq = chess::Square(chess::File(i % 8), chess::Rank(i / 8));
            cached_depression[i] = CalcSquareDepression(sq);
            cached_tilt[i] = CalcWaveTilt(sq);
            cached_wobble_x[i] = CalcWobbleOffset(i, true);
            cached_wobble_y[i] = CalcWobbleOffset(i, false);
            cached_jiggle[i] = CalcJiggleOffset(i);
            
            cached_enemy_shake_x[0][i] = CalcEnemyShakeOffset(sq, false, player_is_white, true);
            cached_enemy_shake_x[1][i] = CalcEnemyShakeOffset(sq, true, player_is_white, true);
            cached_enemy_shake_y[0][i] = CalcEnemyShakeOffset(sq, false, player_is_white, false);
            cached_enemy_shake_y[1][i] = CalcEnemyShakeOffset(sq, true, player_is_white, false);
        }
    }

    EffectsManager() {
        for (int i = 0; i < 150; ++i) {
            int layer = (i < 80) ? 0 : (i < 130 ? 1 : 2); // 80 BG, 50 Mid, 20 FG
            
            float base_speed = (layer == 0) ? 0.3f : (layer == 1 ? 0.8f : 1.8f);
            float base_size = (layer == 0) ? 1.0f : (layer == 1 ? 2.0f : 3.5f);
            
            // Randomly classify the star's color
            ImU32 col = IM_COL32_WHITE;
            int type = rand() % 100;
            if (type < 10) col = IM_COL32(255, 100, 100, 255); // Red Dwarf
            else if (type < 30) col = IM_COL32(100, 200, 255, 255); // Blue Giant
            else if (type < 60) col = IM_COL32(255, 255, 150, 255); // Yellow Sun
            
            stars.push_back({
                ImVec2((float)(rand() % 2000 - 1000), (float)(rand() % 2000 - 1000)),
                base_speed * ((float)(rand() % 50 + 75) / 100.0f), // Speed variation
                base_size * ((float)(rand() % 50 + 75) / 100.0f),  // Size variation
                layer,
                col
            });
        }
    }

    bool HasActiveEffects() const {
        return !particles.empty() || !ghosts.empty() || !active_firework_instances.empty() ||
               !shockwaves.empty() || !pending_effects.empty() || trauma > 0.0 ||
               jiggle_timer > 0.0 || wobble_timer > 0.0 || victory_timer > 0.0 ||
               !flashes.empty() || !squashes.empty() || !ripples.empty() || !impact_wobbles.empty() || !shooting_stars.empty();
    }

    void UpdateHover(chess::Square sq) {
        current_hovered_sq = sq;
    }

    void Update(float dt, int current_combo = 0) {
        if (disable_all_effects) {
            trauma = 0.0;
            return;
        }

        double d = (dt > 0.0f) ? (double)dt : 0.016666667;
        double current_time = ImGui::GetTime();

        // Calculate dynamic starfield speed
        float target_speed = starfield_speed + (current_combo / 3) * 0.1f;
        if (target_speed > 2.5f) {
            target_speed = 2.5f;
            // TODO: Max starfield speed reached. Add placeholder effect here later.
        }
        
        if (current_dynamic_starfield_speed < 0.0f) {
            current_dynamic_starfield_speed = starfield_speed;
        }
        
        // Smoothly interpolate current dynamic speed to target speed
        // If combo breaks, it will gracefully fall back to base starfield_speed
        current_dynamic_starfield_speed += (target_speed - current_dynamic_starfield_speed) * 2.0f * (float)d;

        // Update shooting stars
        for (size_t i = 0; i < shooting_stars.size(); ) {
            shooting_stars[i].life -= (float)d;
            shooting_stars[i].pos.x += shooting_stars[i].vel.x * (float)d;
            shooting_stars[i].pos.y += shooting_stars[i].vel.y * (float)d;
            if (shooting_stars[i].life <= 0.0f) shooting_stars.erase(shooting_stars.begin() + i);
            else ++i;
        }

        // Spawn Comets / Meteor Showers dynamically based on combo
        int spawn_chance = std::max(20, 1000 - (current_combo * 25)); // Smoothly increase frequency

        if (enable_starfield && !disable_all_effects && (rand() % spawn_chance == 0)) {
            // Meteor showers typically have a radiant (come from a similar direction)
            // Let's make them flow diagonally (e.g., top-right to bottom-left) with a small angle variance
            float base_angle = 3.14159f * 1.2f; // Down and left
            float angle = base_angle + ((float)(rand() % 60 - 30) * 3.14159f / 180.0f);
            
            float speed = (float)(rand() % 500 + 800) * (1.0f + (current_combo * 0.02f)); // Slower, more realistic
            float life = 0.8f + (float)(rand() % 80) / 100.0f; // Longer life
            shooting_stars.push_back({
                ImVec2((float)(rand() % 2000 - 1000), (float)(rand() % 2000 - 1000)),
                ImVec2(cosf(angle) * speed, sinf(angle) * speed),
                life, life
            });
        }

        // Spawn Celestial Bodies on combo milestones or randomly at high combo
        if ((current_combo >= 10 && current_combo >= last_celestial_combo + 10) || 
            (current_combo >= 15 && enable_starfield && !disable_all_effects && (rand() % 800 == 0))) {
            last_celestial_combo = (current_combo / 10) * 10;
            if (enable_starfield && !disable_all_effects) {
                float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
                float start_dist = (float)(rand() % 100 + 20); // Start very close to center so they fly out
                
                int rand_type = rand() % 100;
                CelestialType t = CelestialType::PLANET;
                ImU32 col = IM_COL32(rand() % 155 + 100, rand() % 155 + 100, rand() % 155 + 100, 255);
                
                if (rand_type < 35) t = CelestialType::PLANET;
                else if (rand_type < 65) t = CelestialType::GAS_GIANT;
                else if (rand_type < 90) t = CelestialType::NEBULA;
                else {
                    t = CelestialType::BLACK_HOLE;
                    if (rand() % 2 == 0) col = IM_COL32(255, 100, 40, 255); // Fiery Orange
                    else col = IM_COL32(40, 200, 255, 255); // Intensely Cyan
                }

                celestial_bodies.push_back({
                    ImVec2(cosf(angle) * start_dist, sinf(angle) * start_dist),
                    (float)(rand() % 40 + 20), // Small, distant radius
                    t, col,
                    (float)(rand() % 30 + 10) / 100.0f, // Much faster parallax 0.1 - 0.4
                    0.0f
                });
            }
        }
        if (current_combo < 10) last_celestial_combo = 0;

        // Update per-square hover elevations
        for (int i = 0; i < 64; ++i) {
            float target = (current_hovered_sq != chess::Square::underlying::NO_SQ && static_cast<int>(current_hovered_sq.index()) == i) ? 4.0f : 0.0f;
            hover_elevations[i] += (target - hover_elevations[i]) * 15.0f * (float)d; 
            if (std::abs(hover_elevations[i]) < 0.01f) hover_elevations[i] = 0.0f;
        }

        if (trauma > 0.0) { trauma -= d * 1.0; if (trauma < 0.001) trauma = 0.0; }
        if (jiggle_timer > 0.0) jiggle_timer -= d;
        if (wobble_timer > 0.0) wobble_timer -= d;
        if (victory_timer > 0.0) victory_timer -= d;

        for (auto it = active_firework_instances.begin(); it != active_firework_instances.end(); ) {
            it->delay -= (float)d;
            if (it->delay <= 0.0f) {
                SpawnFireworkImmediate(it->center, it->color);
                it = active_firework_instances.erase(it);
            } else ++it;
        }

        if (!enable_fireworks && particles.empty() && ghosts.empty() && shockwaves.empty() && flashes.empty() && squashes.empty() && ripples.empty()) return;

        for (auto& p : particles) {
            if (p.has_history) {
                for (int i = 9; i > 0; --i) p.history[i] = p.history[i-1];
                p.history[0] = p.pos;
            }
            p.life -= (float)d;
            p.vel.x *= powf(p.air_resistance, (float)d * 60.0f);
            p.vel.y *= powf(p.air_resistance, (float)d * 60.0f);
            if (current_trail_mode == TrailMode::PARTICLES && !p.has_gravity) {
                float wave = sinf((float)current_time * 5.0f + p.pos.y * 0.05f);
                p.vel.x += wave * 50.0f * (float)d;
            }
            if (p.has_gravity) p.vel.y += p.gravity_strength * (float)d;
            p.pos.x += p.vel.x * (float)d;
            p.pos.y += p.vel.y * (float)d;
        }

        particles.erase(std::remove_if(particles.begin(), particles.end(), [](const Particle& p) { return p.life <= 0; }), particles.end());
        
        auto update_life = [&](auto& vec) {
            for (size_t i = 0; i < vec.size(); ) {
                vec[i].life -= (float)d;
                if (vec[i].life <= 0.0f) vec.erase(vec.begin() + i);
                else ++i;
            }
        };

        update_life(ghosts); update_life(trail_points); update_life(shockwaves);
        update_life(flashes); update_life(ripples); update_life(impact_wobbles);

        for (size_t i = 0; i < squashes.size(); ) {
            squashes[i].timer -= (float)d;
            if (squashes[i].timer <= 0.0f) {
                squashes[i] = squashes.back();
                squashes.pop_back();
            } else ++i;
        }
    }

    void AddTrauma(float amount) { if (!disable_all_effects && enable_shake) trauma = std::min(1.0, trauma + (double)amount); }
    void AddVictory() { if (!disable_all_effects) victory_timer = victory_dance_duration; }
    void AddWobble() { if (enable_wobble) wobble_timer = 0.5; }

    ImVec2 GetShakeOffset() {
        if (disable_all_effects || trauma <= 0.0) return ImVec2(0,0);
        double t = ImGui::GetTime() * 50.0; 
        double intensity = trauma * trauma; 
        return ImVec2((float)(sin(t) * 8.0 * intensity + cos(t * 0.7) * 3.0 * intensity),
                      (float)(cos(t * 0.9) * 8.0 * intensity + sin(t * 1.1) * 3.0 * intensity));
    }
    
    float GetJiggleOffset(int piece_idx) { return cached_jiggle[piece_idx]; }
    float CalcJiggleOffset(int piece_idx) {
        if (disable_all_effects || !enable_physicality || jiggle_timer <= 0.0) return 0.0f;
        float t = (float)(JIGGLE_DURATION - jiggle_timer);
        return 12.0f * phys_bounce_strength * expf(-8.0f * t) * cosf(30.0f * t + (float)piece_idx);
    }
    
    float GetWobbleOffset(int piece_idx, bool horizontal = true) { return horizontal ? cached_wobble_x[piece_idx] : cached_wobble_y[piece_idx]; }
    float CalcWobbleOffset(int piece_idx, bool horizontal = true) {
        if (disable_all_effects || !enable_physicality || wobble_timer <= 0.0) return 0.0f;
        float intensity = (float)(wobble_timer / 0.5) * 6.0f * phys_shake_intensity;
        double t = ImGui::GetTime();
        if (horizontal) return (float)(sin(t * 41.0 + (double)piece_idx * 0.7) * 0.7 + sin(t * 73.0 + (double)piece_idx * 1.3) * 0.3) * intensity;
        return (float)(sin(t * 47.0 + (double)piece_idx * 0.9) * 0.7 + sin(t * 89.0 + (double)piece_idx * 1.1) * 0.3) * intensity;
    }

    float GetEnemyShakeOffset(chess::Square sq, bool is_white_piece, bool player_is_white, bool horizontal = true) { 
        return horizontal ? cached_enemy_shake_x[is_white_piece ? 1 : 0][static_cast<int>(sq.index())] 
                          : cached_enemy_shake_y[is_white_piece ? 1 : 0][static_cast<int>(sq.index())]; 
    }
    float CalcEnemyShakeOffset(chess::Square sq, bool is_white_piece, bool player_is_white, bool horizontal = true) {
        if (disable_all_effects || !enable_physicality) return 0.0f;
        if (is_white_piece == player_is_white) return 0.0f; // Ensure player pieces never shake
        
        float total_offset = 0.0f;
        double t = ImGui::GetTime();
        
        for (const auto& s : impact_wobbles) {
            if (!s.is_correct_move) continue; // Enemy shake only on correct puzzle moves
            
            if (s.is_white_enemy == is_white_piece) {
                int sq1 = static_cast<int>(s.center.index());
                int f1 = sq1 % 8; int r1 = sq1 / 8;
                int sq2 = static_cast<int>(sq.index());
                int f2 = sq2 % 8; int r2 = sq2 / 8;
                float dist = (float)std::max(std::abs(f1 - f2), std::abs(r1 - r2));
                
                if (dist <= s.radius) {
                    float falloff = 1.0f - (dist / (s.radius + 0.1f));
                    float time_fade = s.life / s.max_life;
                    float shake_power = s.intensity * falloff * time_fade * 4.0f * phys_shake_intensity; // Reduced amplitude
                    
                    int seed = sq2 * 13;
                    // Lowered frequency for a "shiver" rather than a "glitch"
                    if (horizontal) total_offset += (float)(sin(t * 60.0 + seed) * 0.8 + sin(t * 85.0 + seed * 1.3) * 0.2) * shake_power;
                    else total_offset += (float)(sin(t * 70.0 + seed) * 0.8 + sin(t * 95.0 + seed * 1.1) * 0.2) * shake_power;
                }
            }
        }
        return total_offset;
    }

    float GetSquareDepression(chess::Square sq) { return cached_depression[static_cast<int>(sq.index())]; }
    float CalcSquareDepression(chess::Square sq) {
        if (disable_all_effects || !enable_physicality) return 0.0f;
        float dep = 0.0f;
        
        // Primary piece impact
        for (const auto& s : squashes) {
            if (s.sq == sq) {
                float t = 1.0f - (s.timer / 0.15f);
                if (t >= 0.0f && t < 1.0f) {
                    float recovery = 1.0f - t;
                    dep += (recovery * recovery * recovery) * 10.0f * s.intensity * thud_squash_factor;
                }
            }
        }
        
        // Outward wave from impact
        if (enable_domino_wave) {
            for (const auto& s : impact_wobbles) {
                if (!s.is_correct_move) continue; // Restrict strictly to correct puzzle moves
                
                int sq1 = static_cast<int>(s.center.index());
                int f1 = sq1 % 8; int r1 = sq1 / 8;
                int sq2 = static_cast<int>(sq.index());
                int f2 = sq2 % 8; int r2 = sq2 / 8;
                float dist = sqrtf((float)((f1 - f2) * (f1 - f2) + (r1 - r2) * (r1 - r2)));
                
                if (dist > 0.0f) { 
                    float t = 1.0f - (s.life / s.max_life); 
                    float wave_dist = t * domino_wave_speed; 
                    float diff = wave_dist - dist;
                    if (diff > 0.0f && diff < 8.0f) { // Extended range for spring bounce
                        // Underdamped harmonic oscillator for spring physics
                        float angular_freq = 3.14159f * 0.8f; // Oscillation speed
                        float decay_rate = 1.2f / std::max(0.1f, domino_wave_bounce); // Bounce sustain
                        
                        float wave_shape = sinf(diff * angular_freq) * expf(-diff * decay_rate);
                        
                        float falloff = std::max(0.0f, 1.0f - (dist / domino_wave_range)); // Smooth edge fading
                        float intensity = wave_shape * 35.0f * s.intensity * falloff * domino_wave_intensity; 
                        dep -= intensity; // Elevate (negative) and bounce
                    }
                }
            }
        }
        
        // Hover elevation
        if (enable_hover_elevation) {
            dep -= hover_elevations[static_cast<int>(sq.index())];
        }
        
        return dep;
    }

    ImVec2 GetWaveTilt(chess::Square sq) { return cached_tilt[static_cast<int>(sq.index())]; }
    ImVec2 CalcWaveTilt(chess::Square sq) {
        if (disable_all_effects || !enable_physicality || !enable_domino_wave) return ImVec2(0.0f, 0.0f);
        ImVec2 tilt(0.0f, 0.0f);
        
        for (const auto& s : impact_wobbles) {
            if (!s.is_correct_move) continue;
            
            int sq1 = static_cast<int>(s.center.index());
            int f1 = sq1 % 8; int r1 = sq1 / 8;
            int sq2 = static_cast<int>(sq.index());
            int f2 = sq2 % 8; int r2 = sq2 / 8;
            float dist = sqrtf((float)((f1 - f2) * (f1 - f2) + (r1 - r2) * (r1 - r2)));
            
            if (dist > 0.0f) { 
                float t = 1.0f - (s.life / s.max_life); 
                float wave_dist = t * domino_wave_speed;
                float diff = wave_dist - dist;
                if (diff > 0.0f && diff < 8.0f) {
                    float angular_freq = 3.14159f * 0.8f;
                    float decay_rate = 1.2f / std::max(0.1f, domino_wave_bounce);
                    // Cosine gives us the derivative (slope) of the sine wave
                    float slope = cosf(diff * angular_freq) * expf(-diff * decay_rate);
                    
                    float falloff = std::max(0.0f, 1.0f - (dist / domino_wave_range));
                    
                    // Direction vector to tilt away from the impact center
                    float dir_x = (float)(f2 - f1);
                    float dir_y = (float)(r2 - r1);
                    float length = sqrtf(dir_x * dir_x + dir_y * dir_y);
                    if (length > 0.0f) {
                        dir_x /= length;
                        dir_y /= length;
                    }
                    
                    // Transfer the kinetic slope into a 2D radial tilt vector
                    float tilt_mag = slope * falloff * s.intensity * domino_wave_wobble * 0.45f;
                    tilt.x += dir_x * tilt_mag;
                    tilt.y += dir_y * tilt_mag;
                }
            }
        }
        return tilt;
    }

    ImVec2 GetPieceScale(chess::Square sq) {
        for (const auto& s : squashes) {
            if (s.sq == sq) {
                float t = 1.0f - (s.timer / 0.15f); // 0.0 at impact, 1.0 at end
                if (t >= 1.0f || t < 0.0f) return ImVec2(1,1);
                
                // Clean, dense force compression (no comical elastic bouncy oscillation)
                float recovery = 1.0f - t;
                float squash = (recovery * recovery * recovery) * 0.2f * s.intensity * thud_squash_factor;
                
                // Bulge slightly outwards while squishing downwards
                return ImVec2(1.0f + squash * 0.6f, std::max(0.5f, 1.0f - squash));
            }
        }
        return ImVec2(1,1);
    }

    // (Spawn Methods omitted for brevity, but they remain exactly the same)
    void SpawnFlash(ImVec2 pos, ImU32 color = IM_COL32(255, 255, 255, 120)) {
        if (disable_all_effects) return;
        int alpha = (int)(((color >> 24) & 0xFF) * thud_flash_brightness);
        ImU32 col = (color & 0x00FFFFFF) | ((alpha & 0xFF) << 24);
        flashes.push_back({pos, 0.15f, 0.15f, col});
    }

    void SpawnSquash(chess::Square sq, float intensity) {
        if (disable_all_effects) return;
        float final_intensity = intensity * thud_global_intensity;
        for (auto& s : squashes) {
            if (s.sq == sq) { s.timer = 0.15f; s.intensity = final_intensity; return; }
        }
        squashes.push_back({sq, 0.15f, final_intensity});
    }

    void SpawnRipple(ImVec2 pos, float intensity) {
        if (disable_all_effects) return;
        ripples.push_back({pos, 0.4f, 0.4f, 40.0f * intensity * thud_global_intensity * thud_ripple_scale});
    }

    void SpawnImpactWobble(chess::Square sq, int piece_type, bool is_white, float intensity, bool is_correct_move = false) {
        if (disable_all_effects || !enable_physicality) return;
        
        float radius = 1.5f; // Pawn Default
        if (piece_type == 1 || piece_type == 2) radius = 3.5f; // Knight/Bishop
        else if (piece_type == 3) radius = 5.5f; // Rook
        else if (piece_type == 4 || piece_type == 5) radius = 10.0f; // Queen/King

        impact_wobbles.push_back({sq, domino_wave_duration, domino_wave_duration, radius, intensity * thud_global_intensity, !is_white, is_correct_move});
    }

    void SpawnShatter(ImVec2 pos, ImU32 color, float intensity) {
        if (disable_all_effects) return;
        int count = (int)(25 * intensity * thud_global_intensity * thud_puff_density);
        if (count > 100) count = 100;
        for (int i = 0; i < count; ++i) {
            Particle p;
            p.pos = ImVec2(pos.x + (rand() % 20 - 10), pos.y + (rand() % 10));
            float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
            float speed = (float)(rand() % 400 + 200) * intensity * thud_global_intensity;
            p.vel = ImVec2(cosf(angle) * speed, sinf(angle) * speed * 0.3f);
            
            int r = std::min(255, (int)((color & 0xFF) * 1.5f));
            int g = std::min(255, (int)(((color >> 8) & 0xFF) * 1.5f));
            int b = std::min(255, (int)(((color >> 16) & 0xFF) * 1.5f));
            p.color = IM_COL32(r, g, b, 255);
            
            p.max_life = 0.2f + (float)(rand() % 20) / 100.0f;
            p.life = p.max_life;
            p.size = (float)(rand() % 4 + 1) * thud_puff_density;
            p.initial_size = p.size;
            p.has_gravity = false;
            p.air_resistance = 0.85f;
            p.is_diamond = true; 
            particles.push_back(p);
        }
    }

    void SpawnFirework(ImVec2 center, ImU32 color) {
        if (disable_all_effects || !enable_fireworks) return;
        for (int i = 0; i < fw_count; ++i) {
            active_firework_instances.push_back({ImVec2(center.x + (i == 0 ? 0.0f : ((rand() % 200) - 100.0f)), center.y + (i == 0 ? 0.0f : ((rand() % 200) - 100.0f))), color, (i == 0 ? 0.0f : ((rand() % 100) / 100.0f * 0.5f))});
        }
    }

    void SpawnFireworkImmediate(ImVec2 center, ImU32 color) {
        if (particles.size() >= MAX_PARTICLES) return;
        
        // Spawn a bright central flash when detonating
        flashes.push_back({center, 0.2f, 0.2f, IM_COL32(255, 255, 255, 200)});
        
        if (current_firework_style == FireworkStyle::RINGS || current_firework_style == FireworkStyle::RED_SHELL) {
            int num_rings = 3;
            // Red shell gets 3x particles for a dense, chaotic burst
            int base_particles_per_ring = (current_firework_style == FireworkStyle::RINGS) ? fw_particle_count / num_rings : fw_particle_count;
            
            for (int ring = 0; ring < num_rings; ++ring) {
                float ring_speed_base = (float)(rand() % 150 + 100) * fw_explosion_power * fw_particle_speed * (1.0f - (ring * 0.2f));
                
                for (int i = 0; i < base_particles_per_ring; ++i) {
                    Particle p; p.pos = center;
                    float angle = ((float)i / base_particles_per_ring) * 6.28318f + ((rand() % 20 - 10) / 100.0f);
                    float speed = ring_speed_base * (1.0f + ((rand() % 30) - 15) / 100.0f);
                    
                    ImU32 p_color = color;
                    float var = ((rand() % 30) - 15) / 100.0f; 

                    if (current_firework_style == FireworkStyle::RED_SHELL) {
                        angle = (float)(rand() % 360) * 3.14159f / 180.0f; // Chaotic sphere
                        speed *= 0.9f; // Tone down the global burst speed so it's consistent with other presets
                        
                        // Three layers of color for a glowy, realistic red shell
                        if (ring == 0) {
                            speed *= 2.0f; // Blast outwards to create trails
                            p_color = IM_COL32(255, 20, 20, 255); // Deep glowy red
                        }
                        else if (ring == 1) {
                            speed *= 1.4f; // Expand outwards nicely
                            p_color = IM_COL32(255, 80, 20, 255); // Orange/Red
                        }
                        else {
                            speed *= 0.9f; // Core expands moderately
                            p_color = IM_COL32(255, 220, 100, 255); // Hot yellow/white core
                        }
                    } else if (current_firework_style == FireworkStyle::RINGS && ring == num_rings - 1) {
                        p_color = IM_COL32(255, 255, 255, 255); // Inner ring is white/hot
                    }
                    
                    p.vel = ImVec2(cosf(angle) * speed, sinf(angle) * speed);
                    p.color = IM_COL32(std::clamp((int)((p_color & 0xFF) * (1.0f + var)), 0, 255), std::clamp((int)(((p_color >> 8) & 0xFF) * (1.0f + var)), 0, 255), std::clamp((int)(((p_color >> 16) & 0xFF) * (1.0f + var)), 0, 255), 255);
                    
                    p.max_life = ((float)(rand() % 100) / 100.0f * 0.5f + 0.8f) * fw_duration_scale; 
                    if (current_firework_style == FireworkStyle::RED_SHELL) p.max_life *= 1.4f; // Red shells hang longer
                    p.life = p.max_life;
                    
                    p.size = fw_particle_size * (0.8f + (rand() % 40) / 100.0f) * (1.0f - ring * 0.15f); p.initial_size = p.size;
                    p.has_gravity = true; 
                    p.gravity_strength = DEFAULT_GRAVITY * 0.6f * fw_gravity_scale; 
                    p.air_resistance = 0.94f;
                    
                    if (current_firework_style == FireworkStyle::RED_SHELL) {
                        if (ring == 0) {
                            p.air_resistance = 0.97f; // Moderate drag so it leaves trails but slows down
                            p.has_history = true;
                            for (int h = 0; h < 10; ++h) p.history[h] = p.pos;
                        } else if (ring == 1) {
                            p.air_resistance = 0.94f;  // Higher drag so it hangs better
                        } else {
                            p.air_resistance = 0.92f;  // Core drag
                        }
                        p.gravity_strength *= 0.3f; // Float downwards slowly
                    }
                    
                    p.is_firework = true;
                    particles.push_back(p);
                }
            }
        } else if (current_firework_style == FireworkStyle::GOLDEN_WILLOW) {
            for (int i = 0; i < fw_particle_count; ++i) {
                Particle p; p.pos = center;
                float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
                // Willows explode fast but slow down incredibly quickly
                float speed = (float)(rand() % 400 + 200) * fw_explosion_power * fw_particle_speed; 
                p.vel = ImVec2(cosf(angle) * speed, sinf(angle) * speed);
                float var = ((rand() % 20) - 10) / 100.0f; 
                p.color = IM_COL32(std::clamp((int)(255 * (1.0f + var)), 0, 255), std::clamp((int)(200 * (1.0f + var)), 0, 255), std::clamp((int)(50 * (1.0f + var)), 0, 255), 255);
                p.max_life = ((float)(rand() % 100) / 100.0f * 1.5f + 1.5f) * fw_duration_scale; p.life = p.max_life; // Very long life
                p.size = fw_particle_size * (1.0f + (rand() % 40) / 100.0f); p.initial_size = p.size;
                p.has_gravity = true; p.gravity_strength = DEFAULT_GRAVITY * 1.2f * fw_gravity_scale; p.air_resistance = 0.85f; // High air resistance (drifts down)
                p.is_firework = true;
                particles.push_back(p);
            }
        }
        
        if (current_firework_style != FireworkStyle::GOLDEN_WILLOW) {
            // Add a few chaotic "crackling" particles that shoot out faster and randomly
            for (int i = 0; i < fw_particle_count / 4; ++i) {
                 Particle p; p.pos = center;
                 float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
                 float speed = (float)(rand() % 200 + 300) * fw_explosion_power * fw_particle_speed;
                 p.vel = ImVec2(cosf(angle) * speed, sinf(angle) * speed);
                 p.color = IM_COL32(255, 200, 50, 255); // Gold crackle
                 p.max_life = ((float)(rand() % 100) / 100.0f * 0.3f + 0.3f) * fw_duration_scale; p.life = p.max_life;
                 p.size = fw_particle_size * 0.5f; p.initial_size = p.size;
                 p.has_gravity = true; p.gravity_strength = DEFAULT_GRAVITY * 1.2f * fw_gravity_scale; p.air_resistance = 0.90f;
                 p.is_firework = true;
                 p.is_diamond = true;
                 particles.push_back(p);
            }
        }
    }

    void SpawnGhost(ImVec2 pos, int idx, ImVec2 velocity = {0,0}) {
        if (disable_all_effects || !enable_ghost_move) return;
        if (current_trail_mode == TrailMode::ECHO) ghosts.push_back({pos, idx, trail_life, trail_life});
        else if (current_trail_mode == TrailMode::BLUR) ghosts.push_back({pos, idx, trail_life * 0.5f, trail_life * 0.5f, velocity});
        else if (current_trail_mode == TrailMode::LIGHT_STREAK) trail_points.push_back({ImVec2(pos.x + 37.5f, pos.y + 37.5f), trail_life, trail_life});
        else if (current_trail_mode == TrailMode::PARTICLES) {
            bool is_white = (idx < 6); 
            int count = std::min(6, (int)(3.0f * (1.0f + sqrtf(velocity.x*velocity.x + velocity.y*velocity.y) * 0.005f)));
            for (int i = 0; i < count; ++i) {
                Particle p; p.pos = ImVec2(pos.x + 37.5f + (rand()%30-15), pos.y + 37.5f + (rand()%30-15));
                p.vel = ImVec2(velocity.x * 0.2f + (float)((rand() % 40) - 20), velocity.y * 0.2f + (float)((rand() % 40) - 20));
                p.color = is_white ? IM_COL32(255, 220, 100, 200) : IM_COL32(180, 100, 255, 200);
                p.is_diamond = is_white ? (rand() % 3 == 0) : (rand() % 5 == 0);
                p.max_life = (float)(rand()%50)/100.0f + 0.3f; p.life = p.max_life;
                p.size = (float)(rand()%4 + 2); p.has_gravity = false; p.air_resistance = 0.92f;
                particles.push_back(p);
            }
        }
    }

    void SpawnTrail(ImVec2 pos, ImU32 color = IM_COL32(255, 100, 0, 200)) {
        if (disable_all_effects || !enable_trail || particles.size() >= MAX_PARTICLES) return;
        double current_time = ImGui::GetTime();
        for (int i = 0; i < trail_particle_count; ++i) {
            Particle p; p.pos = ImVec2(pos.x + (float)((rand() % 30) - 15), pos.y + (float)((rand() % 30) - 15));
            if (current_trail_style == TrailStyle::SPARKS) {
                float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
                float speed = (float)(rand() % 80 + 30) * trail_speed_scale;
                p.vel = ImVec2(cosf(angle) * speed, sinf(angle) * speed);
                int rng = rand() % 3; p.color = rng == 0 ? IM_COL32(255, 255, 100, 200) : (rng == 1 ? IM_COL32(255, 150, 0, 200) : IM_COL32(255, 80, 0, 200));  
                p.max_life = (float)(rand() % 60) / 100.0f + 0.5f; p.gravity_strength = -150.0f; p.is_square = false;
            } else if (current_trail_style == TrailStyle::CYBER) {
                p.vel = ImVec2((float)((rand() % 100) - 50) * trail_speed_scale, (float)((rand() % 100) - 50) * trail_speed_scale);
                p.color = (rand() % 2 == 0) ? IM_COL32(0, 255, 255, 200) : IM_COL32(255, 0, 255, 200);
                p.max_life = 0.4f; p.gravity_strength = 0.0f; p.air_resistance = 0.9f; p.is_square = true;
            } else if (current_trail_style == TrailStyle::ICE) {
                p.vel = ImVec2((float)((rand() % 40) - 20) * trail_speed_scale, (float)(rand() % 50 + 20) * trail_speed_scale); 
                p.color = (rand() % 2 == 0) ? IM_COL32(200, 230, 255, 200) : IM_COL32(255, 255, 255, 200);
                p.max_life = 0.8f; p.gravity_strength = 100.0f; p.air_resistance = 0.98f; p.is_square = false;
            } else if (current_trail_style == TrailStyle::WATER) {
                p.vel = ImVec2((float)sin(current_time * 10.0 + (double)i) * 30.0f * trail_speed_scale, (float)(rand() % 30 + 10) * trail_speed_scale);
                int rng = rand() % 3; p.color = rng == 0 ? IM_COL32(0, 150, 255, 150) : (rng == 1 ? IM_COL32(100, 200, 255, 120) : IM_COL32(200, 240, 255, 180)); 
                p.max_life = (float)(rand() % 50) / 100.0f + 0.6f; p.gravity_strength = 50.0f; p.air_resistance = 0.95f; p.is_square = false;
            } else if (current_trail_style == TrailStyle::FIRE) {
                if (rand() % 3 == 0) {
                    p.color = IM_COL32(60, 60, 60, 150); p.max_life = (float)(rand() % 100) / 100.0f + 1.0f; p.size = (float)(rand() % 5 + 4); p.gravity_strength = -250.0f; 
                } else {
                    int rng = rand() % 3; p.color = rng == 0 ? IM_COL32(255, 200, 0, 220) : (rng == 1 ? IM_COL32(255, 100, 0, 220) : IM_COL32(255, 50, 0, 220)); 
                    p.max_life = (float)(rand() % 60) / 100.0f + 0.4f; p.size = (float)(rand() % 3 + 2); p.gravity_strength = -150.0f; 
                }
                float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
                p.vel = ImVec2(cosf(angle) * (float)(rand() % 60 + 20) * trail_speed_scale, sinf(angle) * (float)(rand() % 60 + 20) * trail_speed_scale);
                p.air_resistance = 0.94f; p.is_square = false;
            }
            p.life = p.max_life; p.size = ((float)(rand() % 3 + 2)) * trail_size_scale; p.initial_size = p.size; p.has_gravity = true;
            particles.push_back(p);
        }
    }

    void SpawnSparks(ImVec2 center, ImU32 base_color = IM_COL32(255, 200, 50, 255)) {
        if (disable_all_effects || particles.size() >= MAX_PARTICLES) return;
        for (int i = 0; i < 30; ++i) {
            Particle p; p.pos = center;
            float angle = (float)(rand() % 360) * 3.14159f / 180.0f, speed = (float)(rand() % 200 + 80);
            p.vel = ImVec2(cosf(angle) * speed, sinf(angle) * speed); float var = ((rand() % 30) - 15) / 100.0f; 
            p.color = IM_COL32(std::clamp((int)((base_color & 0xFF) * (1.0f + var)), 0, 255), std::clamp((int)(((base_color >> 8) & 0xFF) * (1.0f + var)), 0, 255), std::clamp((int)(((base_color >> 16) & 0xFF) * (1.0f + var)), 0, 255), 255);
            p.max_life = (float)(rand() % 100) / 100.0f * 0.3f + 0.4f; p.life = p.max_life;
            p.size = (float)(rand() % 3 + 2); p.initial_size = p.size; p.has_gravity = true; p.gravity_strength = DEFAULT_GRAVITY * 1.5f; p.air_resistance = 0.92f; 
            particles.push_back(p);
        }
    }

    void SpawnEmbers(ImVec2 board_tl, float board_sz, ImU32 base_color, int intensity) {
        if (disable_all_effects || particles.size() >= MAX_PARTICLES) return;
        for (int i = 0; i < intensity; ++i) {
            Particle p; 
            float speed_mult = 1.0f + (intensity * 0.1f);
            
            // Core bright color for magical spark
            int r = std::clamp((int)(base_color & 0xFF) + 50 + (rand() % 50 - 25), 0, 255);
            int g = std::clamp((int)((base_color >> 8) & 0xFF) + 50 + (rand() % 50 - 25), 0, 255);
            int b = std::clamp((int)((base_color >> 16) & 0xFF) + 50 + (rand() % 50 - 25), 0, 255);
            p.color = IM_COL32(r, g, b, 255); 
            
            p.max_life = (float)(rand() % 100) / 100.0f + 0.8f; 
            p.life = p.max_life;
            p.size = (float)(rand() % 4 + 2); 
            p.initial_size = p.size; 
            p.has_gravity = false; 
            p.air_resistance = 0.95f;
            p.is_diamond = (rand() % 2 == 0);
            p.ember_style = (int)current_spark_style;

            if (current_spark_style == SparkStyle::CLASSIC) {
                p.pos = ImVec2(board_tl.x + (float)(rand() % (int)board_sz), board_tl.y + board_sz);
                p.vel = ImVec2((float)((rand() % 60) - 30) * speed_mult, (float)(-(rand() % 150 + 50)) * speed_mult);
                p.max_life = (float)(rand() % 100) / 100.0f + 0.5f; p.life = p.max_life;
                p.color = IM_COL32(std::clamp((int)(base_color & 0xFF) + (rand() % 50 - 25), 0, 255), std::clamp((int)((base_color >> 8) & 0xFF) + (rand() % 50 - 25), 0, 255), std::clamp((int)((base_color >> 16) & 0xFF) + (rand() % 50 - 25), 0, 255), 200);
            } else if (current_spark_style == SparkStyle::MAGICAL) {
                p.pos = ImVec2(board_tl.x + (float)(rand() % (int)board_sz), board_tl.y + (float)(rand() % (int)board_sz));
                float angle = 3.14159f + ((float)(rand() % 120 + 30) / 180.0f) * 3.14159f; 
                float speed = (float)(rand() % 150 + 50) * speed_mult;
                p.vel = ImVec2(cosf(angle) * speed, sinf(angle) * speed);
            } else if (current_spark_style == SparkStyle::RADIAL) {
                if (rand() % 100 < 40) continue; // Reduce quantity by 40%
                ImVec2 center = ImVec2(board_tl.x + board_sz * 0.5f, board_tl.y + board_sz * 0.5f);
                p.pos = ImVec2(center.x + (float)(rand() % 20 - 10), center.y + (float)(rand() % 20 - 10));
                float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
                float speed = (float)(rand() % 500 + 250) * speed_mult;
                p.vel = ImVec2(cosf(angle) * speed, sinf(angle) * speed);
                p.size = 0.0f; // Start invisible
                p.initial_size = (float)(rand() % 3 + 2);
            } else if (current_spark_style == SparkStyle::VORTEX) {
                // Spawn near edges
                float t = (float)(rand() % 100) / 100.0f;
                int edge = rand() % 4;
                if (edge == 0) p.pos = ImVec2(board_tl.x + t * board_sz, board_tl.y); // Top
                else if (edge == 1) p.pos = ImVec2(board_tl.x + t * board_sz, board_tl.y + board_sz); // Bottom
                else if (edge == 2) p.pos = ImVec2(board_tl.x, board_tl.y + t * board_sz); // Left
                else p.pos = ImVec2(board_tl.x + board_sz, board_tl.y + t * board_sz); // Right
                
                ImVec2 center = ImVec2(board_tl.x + board_sz * 0.5f, board_tl.y + board_sz * 0.5f);
                float angle_to_center = atan2f(center.y - p.pos.y, center.x - p.pos.x);
                float swirl_angle = angle_to_center + 1.2f; // Offset to swirl inwards
                float speed = (float)(rand() % 300 + 150) * speed_mult;
                p.vel = ImVec2(cosf(swirl_angle) * speed, sinf(swirl_angle) * speed);
                p.air_resistance = 0.92f;
            }

            particles.push_back(p);
        }
    }

    void SpawnComboTextEmbers(ImVec2 pos, ImVec2 sz, ImU32 col) {
        if (disable_all_effects || particles.size() >= MAX_PARTICLES) return;
        for (int i = 0; i < 20; ++i) {
            Particle p;
            p.pos = ImVec2(pos.x + (float)(rand() % (int)std::max(1.0f, sz.x)), pos.y + (float)(rand() % (int)std::max(1.0f, sz.y)));
            float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
            float speed = (float)(rand() % 120 + 60);
            p.vel = ImVec2(cosf(angle) * speed, sinf(angle) * speed - 150.0f);
            p.color = col;
            p.max_life = 0.4f + (float)(rand() % 60) / 100.0f;
            p.life = p.max_life;
            p.size = (float)(rand() % 5 + 3);
            p.initial_size = p.size;
            p.has_gravity = true;
            p.gravity_strength = 600.0f;
            p.air_resistance = 0.92f;
            particles.push_back(p);
        }
    }

    void SpawnShockwave(ImVec2 pos, ImU32 color = IM_COL32(255, 255, 255, 200), float intensity = 1.0f) {
        if (disable_all_effects || shockwaves.size() >= 20) return;
        shockwaves.push_back({pos, 0.4f, 0.4f, color, 10.0f, 120.0f * intensity});
        int count = (int)(6 * intensity); 
        if (particles.size() < MAX_PARTICLES - count) {
            for (int i = 0; i < count; ++i) {
                Particle p; p.pos = pos;
                float angle = (float)(i * 2.0f * 3.14159f / (float)count);
                p.vel = ImVec2(cosf(angle) * (float)(rand() % 40 + 20) * intensity, sinf(angle) * (float)(rand() % 40 + 20) * intensity);
                p.color = color; p.max_life = 0.6f + (float)(rand()%40)/100.0f; p.life = p.max_life;
                p.size = (float)(rand()%4 + 2) * intensity; p.initial_size = p.size; p.has_gravity = false; p.air_resistance = 4.0f; p.is_square = (rand() % 2 == 0); 
                particles.push_back(p);
            }
        }
    }

    void QueueEffect(EffectType type, chess::Square sq, ImU32 color) { pending_effects.push_back({type, sq, color}); }

    // --- Drawing Components ---

    void DrawStarfield(ImDrawList* draw_list, ImVec2 display_size, int combo, float dt, float activity_factor = 1.0f) {
        if (!enable_starfield || disable_all_effects) return;

        // Scale warp speed by activity so we don't "warp" while idling
        float warp_speed = 1.0f + std::max(0.0f, std::min(3.0f, (float)(combo - 15) * 0.1f)) * activity_factor;
        
        // Map speed directly to activity factor for near-zero idle motion
        float effective_star_speed = activity_factor * current_dynamic_starfield_speed;
        
        ImVec2 center = (starfield_origin.x != 0) ? starfield_origin : ImVec2(display_size.x * 0.5f, display_size.y * 0.5f);
        
        draw_list->AddRectFilled(ImVec2(0,0), display_size, IM_COL32(0, 0, 0, 255));

        float time = (float)ImGui::GetTime();

        // Calculate a global parallax jolt based on board trauma
        float jolt_x = 0.0f;
        float jolt_y = 0.0f;
        if (trauma > 0.0) {
            jolt_x = (float)(sin(time * 50.0) * trauma * 30.0);
            jolt_y = (float)(cos(time * 40.0) * trauma * 30.0);
        }

        // Draw Celestial Bodies (Extreme Parallax Background)
        for (size_t i = 0; i < celestial_bodies.size(); ) {
            auto& cb = celestial_bodies[i];
            float speed_mult = cb.distance_scale * warp_speed * 150.0f * effective_star_speed;
            if (speed_mult < 0.05f) speed_mult = 0.05f;

            ImVec2 dir = cb.pos;
            float dist = sqrtf(dir.x * dir.x + dir.y * dir.y);
            if (dist < 1.0f) dist = 1.0f;
            dir.x /= dist; dir.y /= dist;

            cb.pos.x += dir.x * speed_mult * dt;
            cb.pos.y += dir.y * speed_mult * dt;

            // Apply parallax jolt
            ImVec2 p_offset(jolt_x * 0.05f, jolt_y * 0.05f);
            ImVec2 draw_pos = ImVec2(center.x + cb.pos.x + p_offset.x, center.y + cb.pos.y + p_offset.y);

            // Simulate perspective scaling: starts small at center, grows as it moves out
            float perspective_scale = std::clamp(dist / (std::max(display_size.x, display_size.y) * 0.6f), 0.1f, 4.0f);
            float current_radius = cb.radius * perspective_scale;

            // Fade in/out based on distance
            float alpha_fade = std::clamp((dist - 50.0f) / 200.0f, 0.0f, 1.0f); // fade in from center
            if (dist > std::max(display_size.x, display_size.y) * 1.5f) {
                alpha_fade *= std::clamp(1.0f - (dist - std::max(display_size.x, display_size.y) * 1.5f) / 500.0f, 0.0f, 1.0f);
            }

            if (alpha_fade > 0.0f) {
                int a = (int)(alpha_fade * 255);
                ImU32 col = (cb.base_color & 0x00FFFFFF) | (a << 24);

                // Base colors for details
                int r_base = (cb.base_color & 0xFF);
                int g_base = ((cb.base_color >> 8) & 0xFF);
                int b_base = ((cb.base_color >> 16) & 0xFF);

                if (cb.type == CelestialType::PLANET) {
                    // Soft glowing corona
                    draw_list->AddCircleFilled(draw_pos, current_radius * 1.4f, (col & 0x00FFFFFF) | ((int)(a*0.1f)<<24), 64);
                    draw_list->AddCircleFilled(draw_pos, current_radius * 1.2f, (col & 0x00FFFFFF) | ((int)(a*0.25f)<<24), 64);
                    
                    // Core planet
                    draw_list->AddCircleFilled(draw_pos, current_radius, col, 64);
                    
                    // Surface Details (Craters / Continents)
                    for (int c = 0; c < 4; ++c) {
                        float cx = sinf(((float)cb.base_color * 1.3f) + c * 2.1f) * current_radius * 0.6f;
                        float cy = cosf(((float)cb.base_color * 0.8f) + c * 1.7f) * current_radius * 0.6f;
                        float cr = current_radius * (0.15f + 0.1f * sinf((float)cb.base_color + c));
                        ImU32 detail_col = IM_COL32(r_base/2, g_base/2, b_base/2, (int)(a * 0.4f));
                        draw_list->AddCircleFilled(ImVec2(draw_pos.x + cx, draw_pos.y + cy), cr, detail_col, 32);
                    }

                    // Atmospheric Rim Light (Brighter edge)
                    draw_list->AddCircle(draw_pos, current_radius, IM_COL32(255, 255, 255, (int)(a * 0.3f)), 64, current_radius * 0.05f);

                    // Improved 3D spherical shading (crescent shadow opposite the center/light source)
                    for (int s = 1; s <= 8; ++s) {
                        float shadow_rad = current_radius * (1.0f - s * 0.1f);
                        ImVec2 shadow_offset = ImVec2(draw_pos.x - center.x, draw_pos.y - center.y);
                        float len = sqrtf(shadow_offset.x * shadow_offset.x + shadow_offset.y * shadow_offset.y);
                        if(len > 0) { shadow_offset.x /= len; shadow_offset.y /= len; }
                        ImVec2 shadow_pos = ImVec2(draw_pos.x + shadow_offset.x * current_radius * 0.3f, draw_pos.y + shadow_offset.y * current_radius * 0.3f);
                        draw_list->AddCircleFilled(shadow_pos, shadow_rad, IM_COL32(0, 0, 0, (int)(a * 0.15f)), 64);
                    }
                } else if (cb.type == CelestialType::GAS_GIANT) {
                    // Base sphere
                    draw_list->AddCircleFilled(draw_pos, current_radius, col, 64);
                    
                    // Gas bands (smooth horizontal stripes rotated by a random angle)
                    float tilt = sinf(((float)cb.base_color * 1.1f)) * 0.5f; 
                    ImVec2 axis_x = ImVec2(cosf(tilt), sinf(tilt));
                    ImVec2 axis_y = ImVec2(-sinf(tilt), cosf(tilt));

                    for (int b_idx = -3; b_idx <= 3; ++b_idx) {
                        float band_y = b_idx * (current_radius * 0.25f);
                        float band_h = current_radius * (0.1f + 0.05f * sinf((float)cb.base_color + b_idx));
                        float band_w = sqrtf(std::max(0.0f, current_radius * current_radius - band_y * band_y));
                        if(band_w > 0) {
                            ImVec2 p_center = ImVec2(draw_pos.x + axis_y.x * band_y, draw_pos.y + axis_y.y * band_y);
                            ImU32 band_col = (b_idx % 2 == 0) ? IM_COL32(r_base/2, g_base/2, b_base/2, (int)(a * 0.6f)) : IM_COL32(std::min(255, r_base+50), std::min(255, g_base+50), std::min(255, b_base+50), (int)(a * 0.4f));
                            draw_list->AddLine(ImVec2(p_center.x - axis_x.x * band_w, p_center.y - axis_x.y * band_w), 
                                               ImVec2(p_center.x + axis_x.x * band_w, p_center.y + axis_x.y * band_w), 
                                               band_col, band_h);
                        }
                    }

                    // Shadow overlay 
                    for (int s = 1; s <= 6; ++s) {
                        float shadow_rad = current_radius * (1.0f - s * 0.15f);
                        ImVec2 shadow_offset = ImVec2(draw_pos.x - center.x, draw_pos.y - center.y);
                        float len = sqrtf(shadow_offset.x * shadow_offset.x + shadow_offset.y * shadow_offset.y);
                        if(len > 0) { shadow_offset.x /= len; shadow_offset.y /= len; }
                        ImVec2 shadow_pos = ImVec2(draw_pos.x + shadow_offset.x * current_radius * 0.3f, draw_pos.y + shadow_offset.y * current_radius * 0.3f);
                        draw_list->AddCircleFilled(shadow_pos, shadow_rad, IM_COL32(0, 0, 0, (int)(a * 0.2f)), 64);
                    }
                    
                    // Improved planetary rings (multi-layered ellipses)
                    float ring_tilt_angle = tilt + 0.4f; 
                    
                    int segments = 64;
                    for(int r_idx = 0; r_idx < 2; ++r_idx) {
                        float r_rad = current_radius * (1.5f + r_idx * 0.4f);
                        float r_thick = current_radius * (0.15f - r_idx * 0.05f);
                        ImU32 r_col = (r_idx == 0) ? IM_COL32(std::min(255, r_base+50), std::min(255, g_base+50), std::min(255, b_base+50), (int)(a*0.6f)) : (col & 0x00FFFFFF) | ((int)(a*0.4f)<<24);
                        
                        draw_list->PathClear();
                        for(int i = 0; i <= segments; ++i) {
                            float theta = (i / (float)segments) * 6.28318f;
                            float ex = r_rad * cosf(theta);
                            float ey = r_rad * 0.3f * sinf(theta);
                            float rx = ex * cosf(ring_tilt_angle) - ey * sinf(ring_tilt_angle);
                            float ry = ex * sinf(ring_tilt_angle) + ey * cosf(ring_tilt_angle);
                            draw_list->PathLineTo(ImVec2(draw_pos.x + rx, draw_pos.y + ry));
                        }
                        draw_list->PathStroke(r_col, ImDrawFlags_Closed, r_thick);
                    }
                } else if (cb.type == CelestialType::NEBULA) {
                    // Nebulas should be much larger but extremely diffuse and transparent
                    float neb_radius = current_radius * 4.0f; 
                    
                    // Draw multiple overlapping cloud clusters
                    for(int cluster = 0; cluster < 3; ++cluster) {
                        float cx = sinf(((float)cb.base_color * 1.5f) + cluster * 3.1f) * neb_radius * 0.5f;
                        float cy = cosf(((float)cb.base_color * 0.9f) + cluster * 2.7f) * neb_radius * 0.5f;
                        ImVec2 c_pos = ImVec2(draw_pos.x + cx, draw_pos.y + cy);
                        
                        for(int n = 0; n < 5; ++n) {
                            float r_nebula = neb_radius * (0.8f - n * 0.15f);
                            ImVec2 layer_pos = ImVec2(c_pos.x + sinf(n * 2.0f + cluster) * neb_radius * 0.2f, c_pos.y + cosf(n * 2.0f + cluster) * neb_radius * 0.2f);
                            
                            int r_cloud = (cluster == 1) ? std::min(255, r_base + 50) : r_base;
                            int b_cloud = (cluster == 2) ? std::min(255, b_base + 50) : b_base;
                            ImU32 cloud_col = IM_COL32(r_cloud, g_base, b_cloud, 255);
                            
                            int layer_a = (int)(a * 0.05f); // Very transparent
                            draw_list->AddCircleFilled(layer_pos, r_nebula, (cloud_col & 0x00FFFFFF) | (layer_a << 24), 32);
                        }
                    }
                    // Star formation core (bright center)
                    draw_list->AddCircleFilled(draw_pos, current_radius * 0.8f, IM_COL32(255, 255, 255, (int)(a * 0.15f)), 32);
                } else if (cb.type == CelestialType::BLACK_HOLE) {
                    // Relativistic Jets (drawn behind the hole)
                    float tilt = sinf((float)cb.base_color * 1.3f) * 0.7f;
                    ImVec2 jet_dir = ImVec2(sinf(tilt), -cosf(tilt));
                    float jet_len = current_radius * 8.0f;
                    ImU32 jet_col = IM_COL32(r_base, g_base, b_base, (int)(a * 0.3f));
                    draw_list->AddLine(ImVec2(draw_pos.x - jet_dir.x * jet_len, draw_pos.y - jet_dir.y * jet_len),
                                       ImVec2(draw_pos.x + jet_dir.x * jet_len, draw_pos.y + jet_dir.y * jet_len),
                                       jet_col, current_radius * 0.5f);
                    
                    // Accretion Disk (Tilted ellipse behind the core, and in front)
                    float ring_tilt = tilt + 1.5708f; // Perpendicular to jets
                    int segments = 64;
                    for (int r_idx = 0; r_idx < 3; ++r_idx) {
                        float r_rad = current_radius * (1.8f + r_idx * 0.6f);
                        float r_thick = current_radius * (0.8f - r_idx * 0.2f);
                        ImU32 disk_col = IM_COL32(r_base, g_base, b_base, (int)(a * (0.6f - r_idx * 0.15f)));
                        
                        draw_list->PathClear();
                        for(int i = 0; i <= segments; ++i) {
                            float theta = (i / (float)segments) * 6.28318f;
                            float ex = r_rad * cosf(theta);
                            float ey = r_rad * 0.2f * sinf(theta); // highly squashed
                            float rx = ex * cosf(ring_tilt) - ey * sinf(ring_tilt);
                            float ry = ex * sinf(ring_tilt) + ey * cosf(ring_tilt);
                            draw_list->PathLineTo(ImVec2(draw_pos.x + rx, draw_pos.y + ry));
                        }
                        draw_list->PathStroke(disk_col, ImDrawFlags_Closed, r_thick);
                    }

                    // Photon Ring (Bright inner edge)
                    draw_list->AddCircle(draw_pos, current_radius * 1.2f, IM_COL32(std::min(255, r_base+100), std::min(255, g_base+100), std::min(255, b_base+100), (int)(a * 0.8f)), 64, current_radius * 0.15f);

                    // The Void (Event Horizon) - Pure black, occludes everything in the center
                    draw_list->AddCircleFilled(draw_pos, current_radius, IM_COL32(0, 0, 0, a), 64);
                }
            }

            if (dist > std::max(display_size.x, display_size.y) * 2.0f) {
                celestial_bodies.erase(celestial_bodies.begin() + i);
            } else {
                ++i;
            }
        }

        int r, g, b;
        if (combo < 30) { float t = (combo-20)/10.0f; r = 200; g = (int)(t*100); b = 255; }
        else { float t = std::min(1.0f, (combo-30)/10.0f); r = 255; g = 100 + (int)(t*155); b = (int)(255-t*155); }

        for (auto& star : stars) {
            float speed_mult = star.speed * warp_speed * 150.0f * effective_star_speed;
            
            // Ensure even the slowest stars have a tiny bit of math movement, set to 0.1 as requested
            if (speed_mult < 0.1f) speed_mult = 0.1f; 

            ImVec2 dir = star.pos;
            float dist = sqrtf(dir.x * dir.x + dir.y * dir.y);
            if (dist < 1.0f) dist = 1.0f;
            dir.x /= dist; dir.y /= dist;

            star.pos.x += dir.x * speed_mult * dt;
            star.pos.y += dir.y * speed_mult * dt;

            // Reset stars that go off-screen
            if (std::abs(star.pos.x) > display_size.x || std::abs(star.pos.y) > display_size.y) {
                float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
                
                // Spawn exactly at the vanishing point
                float start_dist = (float)(rand() % 20 + 1);
                
                star.pos = ImVec2(cosf(angle) * start_dist, sinf(angle) * start_dist);
                
                // Recalculate dist and dir after reset to prevent the 1-frame 100% opacity glitch
                dir = star.pos;
                dist = sqrtf(dir.x * dir.x + dir.y * dir.y);
                if (dist < 1.0f) dist = 1.0f;
                dir.x /= dist; dir.y /= dist;
            }

            // Apply layer-based parallax depth to the jolt
            float layer_parallax = (star.layer == 0) ? 0.1f : (star.layer == 1 ? 0.4f : 1.0f);
            ImVec2 p_offset(jolt_x * layer_parallax, jolt_y * layer_parallax);

            float trail_length = speed_mult * 0.03f * starfield_trail_size;
            if (trail_length < 1.0f) trail_length = 1.0f;

            ImVec2 p1 = ImVec2(center.x + star.pos.x - dir.x * trail_length + p_offset.x, center.y + star.pos.y - dir.y * trail_length + p_offset.y);
            ImVec2 p2 = ImVec2(center.x + star.pos.x + p_offset.x, center.y + star.pos.y + p_offset.y);
            
            // Start completely invisible at the center and fade in smoothly over a large distance
            float alpha_base = std::clamp(dist / 600.0f, 0.0f, 1.0f);
            float alpha_fade = alpha_base;

            float thickness = star.size * alpha_base * (1.0f + warp_speed * 0.05f) * (1.0f + (starfield_trail_size - 1.0f) * 0.3f);
            
            // Blend combo background tint with intrinsic star color
            int star_r = std::clamp((int)(((star.base_color & 0xFF) + r) * 0.6f), 0, 255);
            int star_g = std::clamp((int)((((star.base_color >> 8) & 0xFF) + g) * 0.6f), 0, 255);
            int star_b = std::clamp((int)((((star.base_color >> 16) & 0xFF) + b) * 0.6f), 0, 255);
            
            draw_list->AddLine(p1, p2, IM_COL32(star_r, star_g, star_b, (int)(alpha_fade * 180)), thickness);
            
            float extra_tail = std::max(0.0f, std::min(1.0f, (float)(combo - 20) / 15.0f));
            if (extra_tail > 0.0f) {
                float core_length = trail_length * 0.4f * extra_tail;
                draw_list->AddLine(p2, ImVec2(p2.x - dir.x * core_length, p2.y - dir.y * core_length), IM_COL32(255, 255, 255, (int)(alpha_fade * 255 * extra_tail)), thickness * 0.6f);
            }
        }

        // Draw Upgraded Shooting Stars (Comets)
        for (const auto& ss : shooting_stars) {
            float fade = ss.life / ss.max_life;
            
            // Fade in smoothly at the start, fade out at the end
            float visual_fade = sinf(fade * 3.14159f); 

            ImVec2 p1 = ImVec2(center.x + ss.pos.x, center.y + ss.pos.y);
            
            // Velocity-based tail length
            float tail_len = sqrtf(ss.vel.x * ss.vel.x + ss.vel.y * ss.vel.y) * 0.15f;
            ImVec2 dir = ImVec2(ss.vel.x, ss.vel.y);
            float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
            if (len > 0.0f) { dir.x /= len; dir.y /= len; }

            // Determine comet color based on its unique memory address (pseudo-random but stable for the comet)
            size_t pseudo_rand = (size_t)&ss;
            bool is_cyan = (pseudo_rand % 2 == 0);
            
            ImU32 head_col = is_cyan ? IM_COL32(180, 230, 255, (int)(visual_fade * 255)) : IM_COL32(255, 200, 150, (int)(visual_fade * 255));
            ImU32 tail_col = is_cyan ? IM_COL32(50, 150, 255, (int)(visual_fade * 180)) : IM_COL32(255, 100, 50, (int)(visual_fade * 180));

            // Tapered Tail using multiple segments
            int segments = 5;
            for (int j = 0; j < segments; ++j) {
                float t1 = (float)j / segments;
                float t2 = (float)(j + 1) / segments;
                
                ImVec2 seg_p1 = ImVec2(p1.x - dir.x * tail_len * t1, p1.y - dir.y * tail_len * t1);
                ImVec2 seg_p2 = ImVec2(p1.x - dir.x * tail_len * t2, p1.y - dir.y * tail_len * t2);
                
                float alpha_mult = 1.0f - t1;
                float thick = 5.0f * visual_fade * alpha_mult;
                
                ImU32 seg_color = (tail_col & 0x00FFFFFF) | ((int)(((tail_col >> 24) & 0xFF) * alpha_mult) << 24);
                draw_list->AddLine(seg_p1, seg_p2, seg_color, thick);
            }
            
            // Glowing Head
            draw_list->AddCircleFilled(p1, 2.5f * visual_fade, IM_COL32(255, 255, 255, (int)(visual_fade * 255)), 12);
            draw_list->AddCircleFilled(p1, 8.0f * visual_fade, head_col, 12); // Bloom
        }
    }

    static void GetComboColor(int c, int& r, int& g, int& b) {
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
    }

    void DrawComboEffects(ImDrawList* draw_list, ImVec2 tl, float sz, int combo) {
        if (disable_all_effects || !enable_combo_heat || combo < 3) return;

        int r, g, b;
        GetComboColor(combo, r, g, b);
        
        int intensity = std::min(6, 1 + (combo - 3) / 5);

        SpawnEmbers(tl, sz, IM_COL32(r, g, b, 255), (int)(intensity * heat_intensity));

        if (combo >= 15 && enable_shake) {
             trauma = std::max(trauma, (double)(std::min(0.5f, (float)(combo - 15) * 0.02f) * combo_shake_intensity));
        }
    }

    void DrawFlashes(ImDrawList* draw_list, float square_size) {
        if (disable_all_effects) return;
        for (const auto& f : flashes) {
            float fade = f.life / f.max_life;
            float t = 1.0f - fade;
            ImVec2 center = ImVec2(f.pos.x + square_size * 0.5f, f.pos.y + square_size * 0.5f);
            
            // Soft, expanding contact glow (like a subtle puff of air or light)
            float radius = (square_size * 0.3f) + (square_size * 0.4f * t);
            
            // Very low opacity, pure ambient glow
            int base_alpha = (int)(fade * 40); 
            
            // Multi-layered gradient for extreme softness
            for (int i = 0; i < 3; ++i) {
                float r_layer = radius * (1.0f - i * 0.2f);
                int alpha = base_alpha / (i + 1);
                draw_list->AddCircleFilled(center, r_layer, IM_COL32(255, 255, 255, alpha), 32);
            }
        }
    }

    void DrawRipples(ImDrawList* draw_list) {
        if (disable_all_effects) return;
        for (const auto& r : ripples) {
            float fade = r.life / r.max_life;
            // Smooth sine-based ease out for fluid motion
            float ease_out = sinf((1.0f - fade) * 3.14159f * 0.5f); 
            
            float max_rad = r.size * 1.5f;
            float current_radius = r.size * 0.1f + max_rad * ease_out;
            
            // Draw 3 soft concentric rings to simulate a liquid drop
            for (int i = 0; i < 3; ++i) {
                float rad = current_radius - (i * max_rad * 0.15f * ease_out);
                if (rad <= 0.0f) continue;
                
                // Thickness expands and diffuses as the ripple travels
                float thickness = 1.0f + (1.0f - fade) * 8.0f;
                
                // Alpha follows a sine curve: fades in, peaks, and fades out gracefully. Highly transparent.
                int alpha = (int)(sinf(fade * 3.14159f) * 35.0f * (1.0f - i * 0.25f));
                
                // Very subtle, clear water-like tint
                draw_list->AddCircle(r.pos, rad, IM_COL32(230, 245, 255, alpha), 64, thickness);
            }
        }
    }

    void DrawShockwaves(ImDrawList* draw_list) {
        if (disable_all_effects) return;
        for (const auto& s : shockwaves) {
            float fade = s.life / s.max_life;
            float current_radius = s.initial_size + (s.max_size - s.initial_size) * (1.0f - fade);
            int base_alpha = (int)((s.color >> 24) * fade);
            ImU32 base_col = s.color & 0x00FFFFFF;

            draw_list->AddCircle(s.pos, current_radius, base_col | ((int)(base_alpha * 0.3f) << 24), 0, 10.0f * fade);
            draw_list->AddCircle(s.pos, std::max(0.0f, current_radius - 2.0f * fade), base_col | ((int)(base_alpha * 0.6f) << 24), 0, 5.0f * fade);
            draw_list->AddCircle(s.pos, std::max(0.0f, current_radius - 4.0f * fade), base_col | (base_alpha << 24), 0, 2.0f * fade);
        }
    }

    void Draw(ImDrawList* draw_list) {
        if (disable_all_effects) return;
        double current_time = ImGui::GetTime();
        for (const auto& p : particles) {
            float fade = p.life / p.max_life;
            if (fade < 0.3f && sinf((float)current_time * 60.0f + p.pos.x * 0.1f + p.pos.y * 0.1f) > 0.0f) continue; 

            ImU32 col = (p.color & 0x00FFFFFF) | ((int)(fade * 255.0f) << 24); 
            float current_size = p.size * fade; 
            
            if (p.ember_style == (int)SparkStyle::RADIAL) {
                current_size = p.initial_size * sinf(fade * 3.14159f);
                col = (p.color & 0x00FFFFFF) | ((int)(sinf(fade * 3.14159f) * 255.0f) << 24);
            }

            if (p.is_firework) {
                // Draw a beautiful motion blur trail using particle history
                float thickness = current_size * 0.8f;
                ImU32 glow_col = (col & 0x00FFFFFF) | ((int)((col >> 24) * 0.4f) << 24);
                
                if (p.has_history) {
                    ImVec2 prev_point = p.pos;
                    for (int i = 0; i < 10; ++i) {
                        float t_fade = 1.0f - ((float)i / 10.0f);
                        ImU32 seg_col = (col & 0x00FFFFFF) | ((int)(((col >> 24) & 0xFF) * t_fade) << 24);
                        ImU32 seg_glow = (glow_col & 0x00FFFFFF) | ((int)(((glow_col >> 24) & 0xFF) * t_fade) << 24);
                        
                        draw_list->AddLine(prev_point, p.history[i], seg_col, thickness * t_fade);
                        draw_list->AddLine(prev_point, p.history[i], seg_glow, thickness * 2.5f * t_fade);
                        prev_point = p.history[i];
                    }
                }
                
                // Tip: Brightened version of the particle's own color, not pure white
                int tip_r = std::min(255, (int)(p.color & 0xFF) + 50);
                int tip_g = std::min(255, (int)((p.color >> 8) & 0xFF) + 50);
                int tip_b = std::min(255, (int)((p.color >> 16) & 0xFF) + 50);
                ImU32 tip_col = IM_COL32(tip_r, tip_g, tip_b, (int)(fade * 255));
                draw_list->AddCircleFilled(p.pos, current_size * 0.8f, tip_col);
            } else if (p.is_square) {
                draw_list->AddRectFilled(ImVec2(p.pos.x - current_size, p.pos.y - current_size), ImVec2(p.pos.x + current_size, p.pos.y + current_size), col);
            } else if (p.is_diamond) {
                ImVec2 pts[4] = {{p.pos.x, p.pos.y - current_size * 1.5f}, {p.pos.x + current_size * 1.5f, p.pos.y}, {p.pos.x, p.pos.y + current_size * 1.5f}, {p.pos.x - current_size * 1.5f, p.pos.y}};
                draw_list->AddConvexPolyFilled(pts, 4, col);
            } else {
                draw_list->AddCircleFilled(p.pos, current_size, col);
            }
        }
    }
    
    void DrawGhosts(ImDrawList* draw_list, GLuint* textures, float square_size) {
        if (disable_all_effects || !enable_ghost_move) return;
        double current_time = ImGui::GetTime();

        if (current_trail_mode == TrailMode::ECHO) {
            for (const auto& g : ghosts) {
                if (g.piece_idx >= 0 && g.piece_idx < 16 && textures[g.piece_idx] != 0) {
                    float fade = g.life / g.max_life;
                    
                    // Smooth, fast flicker at the very end of life instead of harsh binary clipping
                    float flicker_alpha = 1.0f;
                    if (fade < 0.2f) {
                        flicker_alpha = (sinf((float)current_time * 100.0f + g.pos.x) * 0.5f + 0.5f);
                    }

                    float scale = 0.7f + 0.3f * fade; // Shrink less aggressively
                    ImVec2 center = ImVec2(g.pos.x + square_size * 0.5f, g.pos.y + square_size * 0.5f);
                    ImVec2 half_sz = ImVec2(square_size * 0.5f * scale, square_size * 0.5f * scale);
                    ImVec2 p_min = ImVec2(center.x - half_sz.x, center.y - half_sz.y);
                    ImVec2 p_max = ImVec2(center.x + half_sz.x, center.y + half_sz.y);

                    // Base transparency
                    int base_a = (int)(fade * flicker_alpha * 120);
                    int shift_a = (int)(fade * flicker_alpha * 90);

                    // Colors based on Trail Style
                    ImU32 col_left = IM_COL32(255, 50, 50, shift_a);
                    ImU32 col_right = IM_COL32(50, 255, 255, shift_a);
                    ImU32 col_core = IM_COL32(255, 255, 255, base_a);

                    if (current_trail_style == TrailStyle::SPARKS) {
                        col_left = IM_COL32(255, 200, 50, shift_a); col_right = IM_COL32(255, 100, 0, shift_a); col_core = IM_COL32(255, 255, 200, base_a);
                    } else if (current_trail_style == TrailStyle::ICE) {
                        col_left = IM_COL32(100, 200, 255, shift_a); col_right = IM_COL32(200, 255, 255, shift_a); col_core = IM_COL32(200, 230, 255, base_a);
                    } else if (current_trail_style == TrailStyle::WATER) {
                        col_left = IM_COL32(0, 100, 255, shift_a); col_right = IM_COL32(50, 200, 200, shift_a); col_core = IM_COL32(150, 200, 255, base_a);
                    } else if (current_trail_style == TrailStyle::FIRE) {
                        col_left = IM_COL32(255, 50, 0, shift_a); col_right = IM_COL32(255, 150, 0, shift_a); col_core = IM_COL32(255, 100, 50, base_a);
                    } // CYBER is default (Red/Cyan)

                    float shift = 4.0f * (1.0f - fade); // Increase shift distance slightly

                    draw_list->AddImage((ImTextureID)(intptr_t)textures[g.piece_idx], ImVec2(p_min.x - shift, p_min.y), ImVec2(p_max.x - shift, p_max.y), ImVec2(0,0), ImVec2(1,1), col_left);
                    draw_list->AddImage((ImTextureID)(intptr_t)textures[g.piece_idx], ImVec2(p_min.x + shift, p_min.y), ImVec2(p_max.x + shift, p_max.y), ImVec2(0,0), ImVec2(1,1), col_right);
                    draw_list->AddImage((ImTextureID)(intptr_t)textures[g.piece_idx], p_min, p_max, ImVec2(0,0), ImVec2(1,1), col_core);
                }
            }
        }
        else if (current_trail_mode == TrailMode::BLUR) {
            for (const auto& g : ghosts) {
                if (g.piece_idx >= 0 && g.piece_idx < 16 && textures[g.piece_idx] != 0) {
                    float fade = g.life / g.max_life;
                    if (fade < 0.3f && sin(current_time * 80.0 + (double)g.pos.x) > 0.0) continue; 
                    
                    float skew_strength = (float)trail_blur_density * 0.003f;
                    ImVec2 offset = ImVec2(g.velocity.x * skew_strength, g.velocity.y * skew_strength);
                    
                    draw_list->AddImageQuad((ImTextureID)(intptr_t)textures[g.piece_idx],
                        ImVec2(g.pos.x - offset.x, g.pos.y - offset.y), ImVec2(g.pos.x + square_size - offset.x, g.pos.y - offset.y),
                        ImVec2(g.pos.x + square_size, g.pos.y + square_size), ImVec2(g.pos.x, g.pos.y + square_size),
                        ImVec2(0,0), ImVec2(1,0), ImVec2(1,1), ImVec2(0,1), IM_COL32(255, 255, 255, (int)(fade * 180)));
                }
            }
        }
        else if (current_trail_mode == TrailMode::LIGHT_STREAK) {
            if (trail_points.size() >= 2) {
                for (size_t i = 0; i < trail_points.size() - 1; ++i) {
                    float fade1 = trail_points[i].life / trail_points[i].max_life;
                    
                    ImVec2 p1 = trail_points[i].pos;
                    ImVec2 p2 = trail_points[i+1].pos;
                    
                    float t1 = (float)i / (float)(trail_points.size() - 1);
                    float t2 = (float)(i + 1) / (float)(trail_points.size() - 1);
                    
                    // Sine profile tapers the ends to points (0.0 at both t=0 and t=1)
                    float thick1 = trail_streak_thickness * 6.0f * sinf(t1 * 3.14159f);
                    float thick2 = trail_streak_thickness * 6.0f * sinf(t2 * 3.14159f);
                    
                    ImVec2 dir = ImVec2(p2.x - p1.x, p2.y - p1.y);
                    float len = sqrtf(dir.x*dir.x + dir.y*dir.y);
                    if (len > 0) { dir.x /= len; dir.y /= len; }
                    ImVec2 norm = ImVec2(-dir.y, dir.x);
                    
                    ImVec2 pts_outer[4] = {
                        ImVec2(p1.x + norm.x * thick1, p1.y + norm.y * thick1),
                        ImVec2(p2.x + norm.x * thick2, p2.y + norm.y * thick2),
                        ImVec2(p2.x - norm.x * thick2, p2.y - norm.y * thick2),
                        ImVec2(p1.x - norm.x * thick1, p1.y - norm.y * thick1)
                    };
                    
                    ImVec2 pts_inner[4] = {
                        ImVec2(p1.x + norm.x * thick1 * 0.3f, p1.y + norm.y * thick1 * 0.3f),
                        ImVec2(p2.x + norm.x * thick2 * 0.3f, p2.y + norm.y * thick2 * 0.3f),
                        ImVec2(p2.x - norm.x * thick2 * 0.3f, p2.y - norm.y * thick2 * 0.3f),
                        ImVec2(p1.x - norm.x * thick1 * 0.3f, p1.y - norm.y * thick1 * 0.3f)
                    };

                    draw_list->AddConvexPolyFilled(pts_outer, 4, IM_COL32(0, 200, 255, (int)(fade1 * 120)));
                    draw_list->AddConvexPolyFilled(pts_inner, 4, IM_COL32(255, 255, 255, (int)(fade1 * 255)));
                }
            }
        }
    }
};

NLOHMANN_JSON_SERIALIZE_ENUM( EffectsManager::TrailStyle, {
    {EffectsManager::TrailStyle::SPARKS, "SPARKS"},
    {EffectsManager::TrailStyle::CYBER, "CYBER"},
    {EffectsManager::TrailStyle::ICE, "ICE"},
    {EffectsManager::TrailStyle::WATER, "WATER"},
    {EffectsManager::TrailStyle::FIRE, "FIRE"},
})

NLOHMANN_JSON_SERIALIZE_ENUM( EffectsManager::TrailMode, {
    {EffectsManager::TrailMode::ECHO, "ECHO"},
    {EffectsManager::TrailMode::LIGHT_STREAK, "LIGHT_STREAK"},
    {EffectsManager::TrailMode::PARTICLES, "PARTICLES"},
    {EffectsManager::TrailMode::BLUR, "BLUR"},
})

NLOHMANN_JSON_SERIALIZE_ENUM( EffectsManager::SparkStyle, {
    {EffectsManager::SparkStyle::CLASSIC, "CLASSIC"},
    {EffectsManager::SparkStyle::MAGICAL, "MAGICAL"},
    {EffectsManager::SparkStyle::RADIAL, "RADIAL"},
    {EffectsManager::SparkStyle::VORTEX, "VORTEX"},
})

NLOHMANN_JSON_SERIALIZE_ENUM( EffectsManager::FireworkStyle, {
    {EffectsManager::FireworkStyle::RINGS, "RINGS"},
    {EffectsManager::FireworkStyle::RED_SHELL, "RED_SHELL"},
    {EffectsManager::FireworkStyle::GOLDEN_WILLOW, "GOLDEN_WILLOW"},
})

inline void to_json(nlohmann::json& j, const EffectsManager& t) {
    j = nlohmann::json{
        {"disable_all_effects", t.disable_all_effects}, {"enable_shake", t.enable_shake}, {"enable_fireworks", t.enable_fireworks},
        {"enable_jiggle", t.enable_jiggle}, {"enable_ghost_move", t.enable_ghost_move}, {"enable_pulse", t.enable_pulse},
        {"enable_trail", t.enable_trail}, {"enable_wobble", t.enable_wobble}, {"enable_rotation", t.enable_rotation},
        {"enable_combo_heat", t.enable_combo_heat}, {"enable_thud", t.enable_thud}, {"enable_physicality", t.enable_physicality},
        {"enable_living_check", t.enable_living_check}, {"enable_victory_dance", t.enable_victory_dance}, {"enable_square_colors", t.enable_square_colors},
        {"phys_bounce_strength", t.phys_bounce_strength}, {"phys_shake_intensity", t.phys_shake_intensity}, {"phys_tilt_amount", t.phys_tilt_amount},
        {"victory_dance_type", t.victory_dance_type}, {"victory_dance_intensity", t.victory_dance_intensity}, {"victory_dance_duration", t.victory_dance_duration},
        {"thud_global_intensity", t.thud_global_intensity}, {"thud_squash_factor", t.thud_squash_factor}, {"thud_flash_brightness", t.thud_flash_brightness},
        {"thud_puff_density", t.thud_puff_density}, {"thud_ripple_scale", t.thud_ripple_scale},
        {"board_tint_r", t.board_tint_r}, {"board_tint_g", t.board_tint_g}, {"board_tint_b", t.board_tint_b},
        {"board_hue_shift", t.board_hue_shift}, {"board_brightness", t.board_brightness},
        {"edge_color_r", t.edge_color_r}, {"edge_color_g", t.edge_color_g}, {"edge_color_b", t.edge_color_b},
        {"current_trail_mode", t.current_trail_mode}, {"trail_life", t.trail_life}, {"trail_blur_density", t.trail_blur_density}, {"trail_streak_thickness", t.trail_streak_thickness},
        {"heat_intensity", t.heat_intensity}, {"combo_shake_intensity", t.combo_shake_intensity},
        {"enable_starfield", t.enable_starfield}, {"starfield_speed", t.starfield_speed}, {"starfield_trail_size", t.starfield_trail_size},
        {"fw_particle_count", t.fw_particle_count}, {"fw_explosion_power", t.fw_explosion_power}, {"fw_gravity_scale", t.fw_gravity_scale},
        {"fw_particle_size", t.fw_particle_size}, {"fw_count", t.fw_count}, {"fw_particle_speed", t.fw_particle_speed}, {"fw_duration_scale", t.fw_duration_scale},
        {"trail_speed_scale", t.trail_speed_scale}, {"trail_size_scale", t.trail_size_scale}, {"trail_particle_count", t.trail_particle_count}, {"current_trail_style", t.current_trail_style},
        {"enable_hover_elevation", t.enable_hover_elevation}, {"enable_domino_wave", t.enable_domino_wave}, {"domino_wave_intensity", t.domino_wave_intensity},
        {"domino_wave_range", t.domino_wave_range}, {"domino_wave_speed", t.domino_wave_speed}, {"domino_wave_bounce", t.domino_wave_bounce},
        {"domino_wave_glow", t.domino_wave_glow}, {"domino_wave_wobble", t.domino_wave_wobble}, {"domino_wave_duration", t.domino_wave_duration}, {"idle_delay_seconds", t.idle_delay_seconds}, {"idle_fps_cap", t.idle_fps_cap}
    };
}

inline void from_json(const nlohmann::json& j, EffectsManager& t) {
    if (j.contains("disable_all_effects")) j.at("disable_all_effects").get_to(t.disable_all_effects);
    if (j.contains("enable_shake")) j.at("enable_shake").get_to(t.enable_shake);
    if (j.contains("enable_fireworks")) j.at("enable_fireworks").get_to(t.enable_fireworks);
    if (j.contains("enable_jiggle")) j.at("enable_jiggle").get_to(t.enable_jiggle);
    if (j.contains("enable_ghost_move")) j.at("enable_ghost_move").get_to(t.enable_ghost_move);
    if (j.contains("enable_pulse")) j.at("enable_pulse").get_to(t.enable_pulse);
    if (j.contains("enable_trail")) j.at("enable_trail").get_to(t.enable_trail);
    if (j.contains("enable_wobble")) j.at("enable_wobble").get_to(t.enable_wobble);
    if (j.contains("enable_rotation")) j.at("enable_rotation").get_to(t.enable_rotation);
    if (j.contains("enable_combo_heat")) j.at("enable_combo_heat").get_to(t.enable_combo_heat);
    if (j.contains("enable_thud")) j.at("enable_thud").get_to(t.enable_thud);
    if (j.contains("enable_physicality")) j.at("enable_physicality").get_to(t.enable_physicality);
    if (j.contains("enable_living_check")) j.at("enable_living_check").get_to(t.enable_living_check);
    if (j.contains("enable_victory_dance")) j.at("enable_victory_dance").get_to(t.enable_victory_dance);
    if (j.contains("enable_square_colors")) j.at("enable_square_colors").get_to(t.enable_square_colors);
    
    if (j.contains("phys_bounce_strength")) j.at("phys_bounce_strength").get_to(t.phys_bounce_strength);
    if (j.contains("phys_shake_intensity")) j.at("phys_shake_intensity").get_to(t.phys_shake_intensity);
    if (j.contains("phys_tilt_amount")) j.at("phys_tilt_amount").get_to(t.phys_tilt_amount);
    if (j.contains("victory_dance_type")) j.at("victory_dance_type").get_to(t.victory_dance_type);
    if (j.contains("victory_dance_intensity")) j.at("victory_dance_intensity").get_to(t.victory_dance_intensity);
    if (j.contains("victory_dance_duration")) j.at("victory_dance_duration").get_to(t.victory_dance_duration);
    if (j.contains("thud_global_intensity")) j.at("thud_global_intensity").get_to(t.thud_global_intensity);
    if (j.contains("thud_squash_factor")) j.at("thud_squash_factor").get_to(t.thud_squash_factor);
    if (j.contains("thud_flash_brightness")) j.at("thud_flash_brightness").get_to(t.thud_flash_brightness);
    if (j.contains("thud_puff_density")) j.at("thud_puff_density").get_to(t.thud_puff_density);
    if (j.contains("thud_ripple_scale")) j.at("thud_ripple_scale").get_to(t.thud_ripple_scale);
    
    if (j.contains("board_tint_r")) j.at("board_tint_r").get_to(t.board_tint_r);
    if (j.contains("board_tint_g")) j.at("board_tint_g").get_to(t.board_tint_g);
    if (j.contains("board_tint_b")) j.at("board_tint_b").get_to(t.board_tint_b);
    if (j.contains("board_hue_shift")) j.at("board_hue_shift").get_to(t.board_hue_shift);
    if (j.contains("board_brightness")) j.at("board_brightness").get_to(t.board_brightness);
    if (j.contains("edge_color_r")) j.at("edge_color_r").get_to(t.edge_color_r);
    if (j.contains("edge_color_g")) j.at("edge_color_g").get_to(t.edge_color_g);
    if (j.contains("edge_color_b")) j.at("edge_color_b").get_to(t.edge_color_b);
    
    if (j.contains("current_spark_style")) j.at("current_spark_style").get_to(t.current_spark_style);

    if (j.contains("current_trail_mode")) j.at("current_trail_mode").get_to(t.current_trail_mode);
    if (j.contains("trail_life")) j.at("trail_life").get_to(t.trail_life);
    if (j.contains("trail_blur_density")) j.at("trail_blur_density").get_to(t.trail_blur_density);
    if (j.contains("trail_streak_thickness")) j.at("trail_streak_thickness").get_to(t.trail_streak_thickness);
    
    if (j.contains("heat_intensity")) j.at("heat_intensity").get_to(t.heat_intensity);
    if (j.contains("combo_shake_intensity")) j.at("combo_shake_intensity").get_to(t.combo_shake_intensity);
    if (j.contains("enable_starfield")) j.at("enable_starfield").get_to(t.enable_starfield);
    if (j.contains("starfield_speed")) j.at("starfield_speed").get_to(t.starfield_speed);
    if (j.contains("starfield_trail_size")) j.at("starfield_trail_size").get_to(t.starfield_trail_size);
    
    if (j.contains("current_firework_style")) j.at("current_firework_style").get_to(t.current_firework_style);
    
    if (j.contains("fw_particle_count")) j.at("fw_particle_count").get_to(t.fw_particle_count);
    if (j.contains("fw_explosion_power")) j.at("fw_explosion_power").get_to(t.fw_explosion_power);
    if (j.contains("fw_gravity_scale")) j.at("fw_gravity_scale").get_to(t.fw_gravity_scale);
    if (j.contains("fw_particle_size")) j.at("fw_particle_size").get_to(t.fw_particle_size);
    if (j.contains("fw_count")) j.at("fw_count").get_to(t.fw_count);
    if (j.contains("fw_particle_speed")) j.at("fw_particle_speed").get_to(t.fw_particle_speed);
    if (j.contains("fw_duration_scale")) j.at("fw_duration_scale").get_to(t.fw_duration_scale);
    
    if (j.contains("trail_speed_scale")) j.at("trail_speed_scale").get_to(t.trail_speed_scale);
    if (j.contains("trail_size_scale")) j.at("trail_size_scale").get_to(t.trail_size_scale);
    if (j.contains("trail_particle_count")) j.at("trail_particle_count").get_to(t.trail_particle_count);
    if (j.contains("current_trail_style")) j.at("current_trail_style").get_to(t.current_trail_style);
    
    if (j.contains("enable_hover_elevation")) j.at("enable_hover_elevation").get_to(t.enable_hover_elevation);
    if (j.contains("enable_domino_wave")) j.at("enable_domino_wave").get_to(t.enable_domino_wave);
    if (j.contains("domino_wave_intensity")) j.at("domino_wave_intensity").get_to(t.domino_wave_intensity);
    if (j.contains("domino_wave_range")) j.at("domino_wave_range").get_to(t.domino_wave_range);
    if (j.contains("domino_wave_speed")) j.at("domino_wave_speed").get_to(t.domino_wave_speed);
    if (j.contains("domino_wave_bounce")) j.at("domino_wave_bounce").get_to(t.domino_wave_bounce);
    if (j.contains("domino_wave_glow")) j.at("domino_wave_glow").get_to(t.domino_wave_glow);
    if (j.contains("domino_wave_wobble")) j.at("domino_wave_wobble").get_to(t.domino_wave_wobble);
    if (j.contains("domino_wave_duration")) j.at("domino_wave_duration").get_to(t.domino_wave_duration);
    
    if (j.contains("idle_delay_seconds")) j.at("idle_delay_seconds").get_to(t.idle_delay_seconds);
    if (j.contains("idle_fps_cap")) j.at("idle_fps_cap").get_to(t.idle_fps_cap);
}
