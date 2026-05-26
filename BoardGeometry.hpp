#pragma once
#include <cmath>
#include <imgui.h>
#include "chess.hpp"

namespace BoardGeometry {
    inline void GetVisualFromSquare(chess::Square sq, float cached_cos, float cached_sin, int& out_c, int& out_r) {
        float center = 3.5f;
        float c = (float)sq.file();
        float r = 7.0f - (float)sq.rank(); 
        
        float cx = c - center;
        float cy = r - center;
        
        out_c = (int)round(cx * cached_cos - cy * cached_sin + center);
        out_r = (int)round(cx * cached_sin + cy * cached_cos + center);
    }

    inline chess::Square GetSquareFromVisual(int col, int row, float cached_cos, float cached_sin) {
        float center = 3.5f;
        float rx = (float)col - center;
        float ry = (float)row - center;
        
        float cx = rx * cached_cos - ry * (-cached_sin);
        float cy = rx * (-cached_sin) + ry * cached_cos;
        
        int file = (int)round(cx + center);
        int rank = 7 - (int)round(cy + center);
        
        if (file < 0 || file > 7 || rank < 0 || rank > 7) return chess::Square::underlying::NO_SQ;
        return chess::Square(chess::File(file), chess::Rank(rank));
    }

    inline ImVec2 GetSquareScreenPos(ImVec2 board_top_left, int col, int row, float square_size, ImVec2 shake) {
        return ImVec2(board_top_left.x + (float)col * square_size + shake.x,
                      board_top_left.y + (float)row * square_size + shake.y);
    }

    inline chess::Square GetSquareAtScreenPos(ImVec2 board_top_left, ImVec2 mouse_pos, float square_size, float cached_cos, float cached_sin, ImVec2 shake) {
        float x = mouse_pos.x - board_top_left.x - shake.x;
        float y = mouse_pos.y - board_top_left.y - shake.y;
        
        float center = 4.0f;
        float final_c = x / square_size;
        float final_r = y / square_size;
        
        float rx = final_c - center;
        float ry = final_r - center;
        
        float cx = rx * cached_cos - ry * (-cached_sin);
        float cy = rx * (-cached_sin) + ry * cached_cos;
        
        float c = cx + center;
        float r_visual = cy + center;
        
        int file = (int)floor(c);
        int rank = 7 - (int)floor(r_visual);
        
        if (file < 0 || file > 7 || rank < 0 || rank > 7) return chess::Square::underlying::NO_SQ;
        return chess::Square(chess::File(file), chess::Rank(rank));
    }
}