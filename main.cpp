#include "BejeweledSolver.h"
#include "PuzzleIDs.h"
#include <vector>
#include <unordered_set> // <-- THÊM THƯ VIỆN BẢNG BĂM NÀY
#include <sstream>       // <-- THÊM THƯ VIỆN CHUYỂN ĐỔI CHUỖI
#include <filesystem> // <-- Thư viện này giúp C++ tự lục tìm file trong máy tính
#include <algorithm>
#include <queue>
#include <cmath>

uint8_t currentBpzHeaderPrefix[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// HÀM ĐỌC FILE .BPZ NHỊ PHÂN
bool loadBPZFile(const std::string& filename, BejeweledBoard& board) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Loi: Khong the mo duoc file: " << filename << ". Hay kiem tra xem file co nam dung thu muc khong!" << std::endl;
        return false;
    }

    // Đọc toàn bộ nội dung file vào buffer
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    if (content.size() < 10) {
        std::cerr << "Loi: File qua nho!" << std::endl;
        return false;
    }

    // Đọc 10 byte đầu làm Header Prefix
    for (int i = 0; i < 10; ++i) {
        currentBpzHeaderPrefix[i] = static_cast<uint8_t>(content[i]);
    }
    
    // Byte thứ 5 làm mode phân biệt Đá/Bom
    board.modeOffset04 = static_cast<uint8_t>(content[4]);

    // Kiểm tra và bỏ qua phần metadata nếu file được sinh từ generator của nhóm khác
    size_t startOffset = 5;
    if (content.find("PuzzName") != std::string::npos) {
        startOffset = 40; // Bỏ qua 40 byte đầu (0x28) chứa metadata
    }

    size_t curr = startOffset;
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (curr >= content.size()) {
                board.matrix[r][c].type = EMPTY;
                board.matrix[r][c].skin = 0xFF;
                board.matrix[r][c].isPowered = 0x00;
                board.matrix[r][c].bombCounter = 0;
                continue;
            }

            uint8_t firstByte = static_cast<uint8_t>(content[curr++]);
            if (firstByte == 0xFF) { 
                board.matrix[r][c].type = EMPTY;
                board.matrix[r][c].skin = 0xFF;
                board.matrix[r][c].isPowered = 0x00;
                board.matrix[r][c].bombCounter = 0;
            } 
            else {
                uint8_t currentSkin = firstByte;
                bool isRock = false;
                bool isBomb = false;

                if (board.modeOffset04 == 0x01) {
                    if (currentSkin == 0x07) isRock = true;
                    if (currentSkin == 0x08) isBomb = true;
                } else if (board.modeOffset04 == 0x02) {
                    if (currentSkin == 0x08) isRock = true;
                    if (currentSkin == 0x07) isBomb = true;
                }

                if (isBomb) {
                    board.matrix[r][c].type = BOMB;
                    board.matrix[r][c].skin = currentSkin;
                    if (curr + 2 <= content.size()) {
                        board.matrix[r][c].isPowered = static_cast<uint8_t>(content[curr]);
                        board.matrix[r][c].bombCounter = static_cast<uint8_t>(content[curr+1]);
                        curr += 2;
                    }
                } 
                else if (isRock) {
                    board.matrix[r][c].type = ROCK;
                    board.matrix[r][c].skin = currentSkin;
                    if (curr + 1 <= content.size()) {
                        board.matrix[r][c].isPowered = static_cast<uint8_t>(content[curr]);
                        curr += 1;
                    }
                } 
                else {
                    // Kim cương thường luôn chiếm 2 byte (skin + isPowered)
                    board.matrix[r][c].type = NORMAL;
                    board.matrix[r][c].skin = currentSkin;
                    if (curr + 1 <= content.size()) {
                        board.matrix[r][c].isPowered = static_cast<uint8_t>(content[curr]);
                        curr += 1;
                    }
                }
            }
        }
    }

    std::cout << "Doc file .bpz thanh cong!" << std::endl;
    return true;
}

// HÀM IN BÀN CỜ RA CONSOLE ĐỂ KIỂM TRA
void printBoard(const BejeweledBoard& board) {
    std::cout << "\n=== BAN CO HIEN TAI (8x8) ===\n" << std::endl;
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (board.matrix[r][c].type == EMPTY) {
                std::cout << "[EMPTY] ";
            } else if (board.matrix[r][c].type == BOMB) {
                std::cout << "[B" << (int)board.matrix[r][c].skin << ":" << (int)board.matrix[r][c].bombCounter << "] ";
            } else if (board.matrix[r][c].type == ROCK) {
                std::cout << "[ROCK ] ";
            } else {
                std::cout << "[GEM" << (int)board.matrix[r][c].skin << "] ";
            }
        }
        std::cout << std::endl; // Hết một hàng thì xuống dòng
    }
    std::cout << "=============================" << std::endl;
}
// ==================== CÁC HÀM LOGIC GAME (BƯỚC 2) ====================

// 1. Hàm phụ trợ: Kiểm tra xem tại một ô cụ thể có đang tạo ra Match-3 theo hàng ngang hoặc hàng dọc không
uint8_t getGemColor(const BejeweledBoard& board, int r, int c) {
    const Gem& gem = board.matrix[r][c];
    if (gem.type == NORMAL) return gem.skin;
    if (gem.type == BOMB) return gem.isPowered; // byte isPowered lưu màu sắc quả bom
    return 0xFF; // EMPTY hoặc ROCK không thể match
}

bool isMatchAt(const BejeweledBoard& board, int r, int c) {
    uint8_t color = getGemColor(board, r, c);
    if (color == 0xFF) return false;

    // Kiểm tra hàng ngang (3 viên liên tiếp trùng màu)
    if (c >= 2 && getGemColor(board, r, c-1) == color && getGemColor(board, r, c-2) == color) return true;
    if (c <= 5 && getGemColor(board, r, c+1) == color && getGemColor(board, r, c+2) == color) return true;
    if (c >= 1 && c <= 6 && getGemColor(board, r, c-1) == color && getGemColor(board, r, c+1) == color) return true;

    // Kiểm tra hàng dọc (3 viên liên tiếp trùng màu)
    if (r >= 2 && getGemColor(board, r-1, c) == color && getGemColor(board, r-2, c) == color) return true;
    if (r <= 5 && getGemColor(board, r+1, c) == color && getGemColor(board, r+2, c) == color) return true;
    if (r >= 1 && r <= 6 && getGemColor(board, r-1, c) == color && getGemColor(board, r+1, c) == color) return true;

    return false;
}

bool checkMatchesExist(const BejeweledBoard& board) {
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (isMatchAt(board, r, c)) return true;
        }
    }
    return false;
}

// 2. Hàm kiểm tra lượt tráo đổi giữa (r1, c1) và (r2, c2) có hợp lệ không
bool isValidSwap(BejeweledBoard& board, int r1, int c1, int r2, int c2) {
    // Chỉ được tráo đổi ô kề cạnh (ngang hoặc dọc)
    if (abs(r1 - r2) + abs(c1 - c2) != 1) return false;
    
    // Đá (ROCK) không thể hoán đổi chủ động
    if (board.matrix[r1][c1].type == ROCK || board.matrix[r2][c2].type == ROCK) return false;

    // Không thể hoán đổi hai ô trống (EMPTY)
    if (board.matrix[r1][c1].type == EMPTY && board.matrix[r2][c2].type == EMPTY) return false;

    // Nếu một trong hai ô là EMPTY, lượt tráo đổi LUÔN HỢP LỆ (trượt ngọc vào ô trống)
    if (board.matrix[r1][c1].type == EMPTY || board.matrix[r2][c2].type == EMPTY) {
        return true;
    }

    // Nếu một trong hai viên là Hypercube (isPowered == 0x02)
    bool isHC1 = (board.matrix[r1][c1].type == NORMAL && board.matrix[r1][c1].isPowered == 0x02);
    bool isHC2 = (board.matrix[r2][c2].type == NORMAL && board.matrix[r2][c2].isPowered == 0x02);
    if (isHC1 || isHC2) {
        return true;
    }

    // Thử tráo đổi tạm thời
    std::swap(board.matrix[r1][c1], board.matrix[r2][c2]);

    // Kiểm tra xem sau khi tráo, các ô có ngọc có tạo ra tổ hợp nổ không
    bool valid = false;
    if (board.matrix[r1][c1].type != EMPTY && isMatchAt(board, r1, c1)) valid = true;
    if (board.matrix[r2][c2].type != EMPTY && isMatchAt(board, r2, c2)) valid = true;

    // Tráo ngược trả lại trạng thái cũ để giữ nguyên ma trận ban đầu
    std::swap(board.matrix[r1][c1], board.matrix[r2][c2]);

    return valid;
}

// 3. Hàm thực hiện quét và xóa các tổ hợp Match-3 trở lên
bool executeMatchAndExplosions(BejeweledBoard& board) {
    bool toRemove[8][8] = {false};
    bool foundMatch = false;

    // Quét hàng ngang tìm chuỗi trùng màu >= 3
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 6; ++c) {
            uint8_t color = getGemColor(board, r, c);
            if (color != 0xFF) {
                if (getGemColor(board, r, c+1) == color && getGemColor(board, r, c+2) == color) {
                    toRemove[r][c] = true;
                    toRemove[r][c+1] = true;
                    toRemove[r][c+2] = true;
                    foundMatch = true;
                }
            }
        }
    }

    // Quét hàng dọc tìm chuỗi trùng màu >= 3
    for (int c = 0; c < 8; ++c) {
        for (int r = 0; r < 6; ++r) {
            uint8_t color = getGemColor(board, r, c);
            if (color != 0xFF) {
                if (getGemColor(board, r+1, c) == color && getGemColor(board, r+2, c) == color) {
                    toRemove[r][c] = true;
                    toRemove[r+1][c] = true;
                    toRemove[r+2][c] = true;
                    foundMatch = true;
                }
            }
        }
    }

    if (!foundMatch) return false;

    // Xử lý nổ dây chuyền 3x3 đối với Flame Gem (isPowered == 0x01) và BOMB
    std::vector<std::pair<int, int>> explosionQueue;
    bool exploded[8][8] = {false};

    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (toRemove[r][c]) {
                const Gem& gem = board.matrix[r][c];
                if ((gem.type == NORMAL && gem.isPowered == 0x01) || gem.type == BOMB) {
                    explosionQueue.push_back({r, c});
                    exploded[r][c] = true;
                }
            }
        }
    }

    int queueIndex = 0;
    while (queueIndex < (int)explosionQueue.size()) {
        auto [er, ec] = explosionQueue[queueIndex++];
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                int nr = er + dr;
                int nc = ec + dc;
                if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8) {
                    toRemove[nr][nc] = true;
                    const Gem& nextGem = board.matrix[nr][nc];
                    if ((nextGem.type == NORMAL && nextGem.isPowered == 0x01) || nextGem.type == BOMB) {
                        if (!exploded[nr][nc]) {
                            exploded[nr][nc] = true;
                            explosionQueue.push_back({nr, nc});
                        }
                    }
                }
            }
        }
    }

    // Giải phóng các ô được đánh dấu nổ
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (toRemove[r][c]) {
                board.matrix[r][c].type = EMPTY;
                board.matrix[r][c].skin = 0xFF;
                board.matrix[r][c].isPowered = 0x00;
                board.matrix[r][c].bombCounter = 0;
            }
        }
    }

    return true;
}

void executeMatch(BejeweledBoard& board) {
    executeMatchAndExplosions(board);
}

// 4. Hàm xử lý trọng lực rơi rụng Gem từ trên xuống lấp chỗ trống
void applyGravity(BejeweledBoard& board) {
    // Duyệt qua từng cột từ trái sang phải
    for (int c = 0; c < 8; ++c) {
        // Duyệt từ hàng đáy (hàng 7) ngược lên đỉnh (hàng 0)
        for (int r = 7; r >= 0; --r) {
            if (board.matrix[r][c].type == EMPTY) {
                // Tìm viên Gem gần nhất ở phía trên ô trống này mà không phải là Đá
                int upperRow = r - 1;
                while (upperRow >= 0 && board.matrix[upperRow][c].type == EMPTY) {
                    upperRow--;
                }
                
                // Nếu tìm thấy viên Gem ở trên, kéo nó rơi xuống vị trí ô trống r hiện tại
                if (upperRow >= 0 && board.matrix[upperRow][c].type != ROCK) {
                    board.matrix[r][c] = board.matrix[upperRow][c];
                    board.matrix[upperRow][c].type = EMPTY; // Ô cũ trở thành trống
                    board.matrix[upperRow][c].skin = 0xFF;
                }
            }
        }
    }
}
// 1. Hàm kiểm tra điều kiện thắng: Toàn bộ ma trận phải là ô EMPTY (0xFF)
bool isBoardCleared(const BejeweledBoard& board) {
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (board.matrix[r][c].type != EMPTY) {
                return false; // Vẫn còn Gem hoặc Đá trên bàn cờ
            }
        }
    }
    return true; // Bảng sạch bóng 100%
}
// Hàm chuyển đổi ma trận 8x8 thành một chuỗi String duy nhất để làm khóa băm (Hash Key)
std::string serializeBoard(const BejeweledBoard& board) {
    std::stringstream ss;
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            // Nối mã loại ô và màu sắc của từng ô vào chuỗi
            ss << (int)board.matrix[r][c].type << "," 
               << (int)board.matrix[r][c].skin << "|";
        }
    }
    return ss.str();
}

// 2. Thuật toán Đệ quy kết hợp Quay lui (Backtracking) để giải bài toán
// HÀM GIẢI BÀI TOÁN NÂNG CẤP TỐI ƯU CẮT NHÁNH CẬN
void resolveBoardStates(BejeweledBoard& board) {
    while (executeMatchAndExplosions(board)) {
        applyGravity(board);
    }
}

void triggerHypercube(BejeweledBoard& board, int hr, int hc, int tr, int tc) {
    uint8_t targetColor = getGemColor(board, hr, hc);
    bool toRemove[8][8] = {false};
    toRemove[tr][tc] = true; // Xóa bản thân Hypercube

    if (targetColor != 0xFF) {
        for (int r = 0; r < 8; ++r) {
            for (int c = 0; c < 8; ++c) {
                if (getGemColor(board, r, c) == targetColor) {
                    toRemove[r][c] = true;
                }
            }
        }
    }

    // Xử lý nổ lan kích hoạt Flame Gem/Bomb
    std::vector<std::pair<int, int>> explosionQueue;
    bool exploded[8][8] = {false};

    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (toRemove[r][c]) {
                const Gem& gem = board.matrix[r][c];
                if ((gem.type == NORMAL && gem.isPowered == 0x01) || gem.type == BOMB) {
                    explosionQueue.push_back({r, c});
                    exploded[r][c] = true;
                }
            }
        }
    }

    int queueIndex = 0;
    while (queueIndex < (int)explosionQueue.size()) {
        auto [er, ec] = explosionQueue[queueIndex++];
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                int nr = er + dr;
                int nc = ec + dc;
                if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8) {
                    toRemove[nr][nc] = true;
                    const Gem& nextGem = board.matrix[nr][nc];
                    if ((nextGem.type == NORMAL && nextGem.isPowered == 0x01) || nextGem.type == BOMB) {
                        if (!exploded[nr][nc]) {
                            exploded[nr][nc] = true;
                            explosionQueue.push_back({nr, nc});
                        }
                    }
                }
            }
        }
    }

    // Thực hiện xóa các ô
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (toRemove[r][c]) {
                board.matrix[r][c].type = EMPTY;
                board.matrix[r][c].skin = 0xFF;
                board.matrix[r][c].isPowered = 0x00;
                board.matrix[r][c].bombCounter = 0;
            }
        }
    }

    applyGravity(board);
    resolveBoardStates(board);
}

bool tickBombs(BejeweledBoard& board) {
    bool toRemove[8][8] = {false};
    bool explodedAny = false;

    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (board.matrix[r][c].type == BOMB) {
                if (board.matrix[r][c].bombCounter > 0) {
                    board.matrix[r][c].bombCounter--;
                }
                if (board.matrix[r][c].bombCounter == 0) {
                    toRemove[r][c] = true;
                    explodedAny = true;
                }
            }
        }
    }

    if (!explodedAny) return true;

    // Nổ dây chuyền 3x3 đối với bom đếm ngược về 0
    std::vector<std::pair<int, int>> explosionQueue;
    bool exploded[8][8] = {false};

    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (toRemove[r][c]) {
                explosionQueue.push_back({r, c});
                exploded[r][c] = true;
            }
        }
    }

    int queueIndex = 0;
    while (queueIndex < (int)explosionQueue.size()) {
        auto [er, ec] = explosionQueue[queueIndex++];
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                int nr = er + dr;
                int nc = ec + dc;
                if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8) {
                    toRemove[nr][nc] = true;
                    const Gem& nextGem = board.matrix[nr][nc];
                    if ((nextGem.type == NORMAL && nextGem.isPowered == 0x01) || nextGem.type == BOMB) {
                        if (!exploded[nr][nc]) {
                            exploded[nr][nc] = true;
                            explosionQueue.push_back({nr, nc});
                        }
                    }
                }
            }
        }
    }

    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (toRemove[r][c]) {
                board.matrix[r][c].type = EMPTY;
                board.matrix[r][c].skin = 0xFF;
                board.matrix[r][c].isPowered = 0x00;
                board.matrix[r][c].bombCounter = 0;
            }
        }
    }

    applyGravity(board);
    resolveBoardStates(board);

    return true;
}

struct CandidateMove {
    Move m;
    int priority;
};

bool solvePuzzleOptimized(BejeweledBoard& board, std::vector<Move>& solutionPath, std::unordered_set<std::string>& visitedStates) {
    if (isBoardCleared(board)) {
        return true;
    }

    // Cắt nhánh nếu độ sâu vượt quá 14 (theo giới hạn đệm gợi ý hệ thống)
    if (solutionPath.size() >= 14) {
        return false;
    }

    std::string boardKey = serializeBoard(board);
    if (visitedStates.count(boardKey) > 0) {
        return false;
    }

    std::vector<CandidateMove> candidates;

    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            int nextRows[] = {r, r + 1};
            int nextCols[] = {c + 1, c};

            for (int i = 0; i < 2; ++i) {
                int nr = nextRows[i];
                int nc = nextCols[i];

                if (nr < 8 && nc < 8) {
                    if (isValidSwap(board, r, c, nr, nc)) {
                        BejeweledBoard tempBoard = board;
                        
                        bool isHC1 = (tempBoard.matrix[r][c].type == NORMAL && tempBoard.matrix[r][c].isPowered == 0x02);
                        bool isHC2 = (tempBoard.matrix[nr][nc].type == NORMAL && tempBoard.matrix[nr][nc].isPowered == 0x02);

                        std::swap(tempBoard.matrix[r][c], tempBoard.matrix[nr][nc]);
                        
                        // Đếm số lượng Đá và Bom trước khi giải phóng
                        int oldRocks = 0, oldBombs = 0;
                        for (int tr = 0; tr < 8; ++tr) {
                            for (int tc = 0; tc < 8; ++tc) {
                                if (board.matrix[tr][tc].type == ROCK) oldRocks++;
                                if (board.matrix[tr][tc].type == BOMB) oldBombs++;
                            }
                        }

                        if (isHC1) {
                            triggerHypercube(tempBoard, nr, nc, r, c);
                        } else if (isHC2) {
                            triggerHypercube(tempBoard, r, c, nr, nc);
                        } else {
                            resolveBoardStates(tempBoard);
                        }

                        int newRocks = 0, newBombs = 0;
                        for (int tr = 0; tr < 8; ++tr) {
                            for (int tc = 0; tc < 8; ++tc) {
                                if (tempBoard.matrix[tr][tc].type == ROCK) newRocks++;
                                if (tempBoard.matrix[tr][tc].type == BOMB) newBombs++;
                            }
                        }

                        int rocksDestroyed = oldRocks - newRocks;
                        int bombsDestroyed = oldBombs - newBombs;

                        int emptyCount = 0;
                        for (int tr = 0; tr < 8; ++tr) {
                            for (int tc = 0; tc < 8; ++tc) {
                                if (tempBoard.matrix[tr][tc].type == EMPTY) emptyCount++;
                            }
                        }

                        // Heuristic: Phá đá trước (x1000), sau đó phá bom (x500), sau cùng là tăng tối đa ô trống
                        int priority = rocksDestroyed * 1000 + bombsDestroyed * 500 + emptyCount;
                        candidates.push_back({{r, c, nr, nc}, priority});
                    }
                }
            }
        }
    }

    // Sắp xếp các ứng viên nước đi theo thứ tự ưu tiên giảm dần
    std::sort(candidates.begin(), candidates.end(), [](const CandidateMove& a, const CandidateMove& b) {
        return a.priority > b.priority;
    });

    for (const auto& cand : candidates) {
        BejeweledBoard backupBoard = board;

        bool isHC1 = (board.matrix[cand.m.r1][cand.m.c1].type == NORMAL && board.matrix[cand.m.r1][cand.m.c1].isPowered == 0x02);
        bool isHC2 = (board.matrix[cand.m.r2][cand.m.c2].type == NORMAL && board.matrix[cand.m.r2][cand.m.c2].isPowered == 0x02);

        std::swap(board.matrix[cand.m.r1][cand.m.c1], board.matrix[cand.m.r2][cand.m.c2]);
        
        if (isHC1) {
            triggerHypercube(board, cand.m.r2, cand.m.c2, cand.m.r1, cand.m.c1);
        } else if (isHC2) {
            triggerHypercube(board, cand.m.r1, cand.m.c1, cand.m.r2, cand.m.c2);
        } else {
            resolveBoardStates(board);
        }

        bool bombsOk = tickBombs(board);
        if (bombsOk) {
            solutionPath.push_back(cand.m);
            if (solvePuzzleOptimized(board, solutionPath, visitedStates)) {
                return true;
            }
            solutionPath.pop_back();
        }

        board = backupBoard;
    }

    visitedStates.insert(boardKey);
    return false;
}

bool solvePuzzle(BejeweledBoard& board, std::vector<Move>& solutionPath) {
    std::unordered_set<std::string> visitedStates;
    resolveBoardStates(board); // Giải phóng các match tự động ban đầu
    return solvePuzzleOptimized(board, solutionPath, visitedStates);
}
// HÀM XUẤT FILE LỜI GIẢI NHỊ PHÂN .SOL
bool exportSOLFile(const std::string& filename, const std::vector<Move>& solutionPath) {
    // Mở file ghi ở chế độ nhị phân (ios::binary)
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Loi: Khong the tao duoc file output .sol!" << std::endl;
        return false;
    }

    // 1. GHI HEADER: 14 byte đầu tiên
    // - 4 byte signature: 02 B0 37 13
    // - 2 byte magic: 02 00
    // - 4 byte puzzle ID
    // - 2 byte start state
    // - 2 byte total states
    uint8_t signature[4] = { 0x02, 0xB0, 0x37, 0x13 };
    file.write(reinterpret_cast<char*>(signature), 4);

    uint8_t magic[2] = { 0x02, 0x00 };
    file.write(reinterpret_cast<char*>(magic), 2);

    // Lấy tên file .bpz tương ứng từ tên file .sol để tra cứu ID
    std::string bpzFilename = filename;
    size_t dotPos = bpzFilename.find_last_of('.');
    if (dotPos != std::string::npos) {
        bpzFilename = bpzFilename.substr(0, dotPos) + ".bpz";
    }
    // Chỉ lấy phần tên file chính, bỏ qua đường dẫn thư mục nếu có
    size_t slashPos = bpzFilename.find_last_of("/\\");
    if (slashPos != std::string::npos) {
        bpzFilename = bpzFilename.substr(slashPos + 1);
    }

    uint32_t puzzleID = getOfficialPuzzleID(bpzFilename);
    file.write(reinterpret_cast<char*>(&puzzleID), 4);

    uint32_t numStates = static_cast<uint32_t>(solutionPath.size());
    uint16_t startState = static_cast<uint16_t>(numStates);
    uint16_t totalStates = static_cast<uint16_t>(numStates + 1);

    file.write(reinterpret_cast<char*>(&startState), 2);
    file.write(reinterpret_cast<char*>(&totalStates), 2);

    // 2. GHI CÁC KHỐI TRẠNG THÁI (State Blocks)
    // - Trạng thái 0: Trạng thái kết thúc sạch bảng (0 gợi ý)
    uint8_t stateZeroHints = 0x00;
    file.write(reinterpret_cast<char*>(&stateZeroHints), 1);

    // - Trạng thái j (từ 1 đến N): Mỗi trạng thái có 1 gợi ý dẫn về trạng thái j - 1
    // Nước đi đầu tiên (Move 0) thuộc về State N (Start State) dẫn tới State N-1
    // Nước đi cuối cùng (Move N-1) thuộc về State 1 dẫn tới State 0
    for (uint32_t j = 1; j <= numStates; ++j) {
        uint8_t numHints = 0x01;
        file.write(reinterpret_cast<char*>(&numHints), 1);

        const Move& currentMove = solutionPath[numStates - j];

        // Tính hướng di chuyển
        uint8_t direction = 0x00;
        if (currentMove.c2 > currentMove.c1) {
            direction = 0x00; // Phải (Right)
        } else if (currentMove.c2 < currentMove.c1) {
            direction = 0x01; // Trái (Left)
        } else if (currentMove.r2 > currentMove.r1) {
            direction = 0x02; // Dưới (Down)
        } else if (currentMove.r2 < currentMove.r1) {
            direction = 0x03; // Trên (Up)
        }

        // Tọa độ ô cờ: 0-indexed row-major (từ 0 đến 63)
        uint8_t gemIndex = static_cast<uint8_t>(currentMove.r1 * 8 + currentMove.c1);

        // Byte kết hợp: 2 bit đầu là hướng, 6 bit sau là tọa độ ô cờ
        uint8_t gemIndexAndDirection = (direction << 6) | gemIndex;
        file.write(reinterpret_cast<char*>(&gemIndexAndDirection), 1);

        uint8_t gotoState = static_cast<uint8_t>(j - 1);
        file.write(reinterpret_cast<char*>(&gotoState), 1);

        // Padding: 2 byte 0x00 0x00
        uint8_t padding[2] = { 0x00, 0x00 };
        file.write(reinterpret_cast<char*>(padding), 2);
    }

    file.close();
    std::cout << "[SUCCESS] Da dong goi va xuat file loi giai nhan phan thanh cong!" << std::endl;
    return true;
}
int main() {
    std::cout << "===============================================================" << std::endl;
    std::cout << "   HE THONG GIAI TU DONG BEJEWELED 2 PUZZLE   " << std::endl;
    std::cout << "===============================================================" << std::endl;

    int successCount = 0;
    int totalCount = 0;

    // Chương trình sẽ quét qua tất cả các file nằm trong thư mục hiện tại của bạn
    for (const auto& entry : std::filesystem::directory_iterator(".")) {
        std::string filePath = entry.path().string();
        
        // Nếu phát hiện thấy file nào có đuôi là .bpz (do nhóm 2/5 vừa bỏ vào)
        if (entry.path().extension() == ".bpz") {
            totalCount++;
            
            std::string inputBPZ = entry.path().filename().string();
            // Tự động tạo ra tên file .sol trùng tên hoàn toàn
            std::string outputSOL = entry.path().stem().string() + ".sol"; 

            std::cout << "\n[+] Dang xu ly file thu " << totalCount << ": " << inputBPZ << "..." << std::endl;

            BejeweledBoard myBoard;
            if (!loadBPZFile(inputBPZ, myBoard)) {
                std::cout << "    [-] Bo qua file do loi doc nhap phan." << std::endl;
                continue;
            }
            std::cout << "Initial board state:" << std::endl;
            printBoard(myBoard);

            std::vector<Move> solutionPath;

            // Kích hoạt bộ não giải đố tối ưu siêu tốc
            if (solvePuzzle(myBoard, solutionPath)) {
                std::cout << "    => [SUCCESS] Tim thay loi giai sau " << solutionPath.size() << " buoc!" << std::endl;
                for (size_t i = 0; i < solutionPath.size(); ++i) {
                    std::cout << "       Buoc " << i + 1 << ": Hoan doi (" << solutionPath[i].r1 << ", " << solutionPath[i].c1 
                              << ") voi (" << solutionPath[i].r2 << ", " << solutionPath[i].c2 << ")" << std::endl;
                }
                exportSOLFile(outputSOL, solutionPath);
                successCount++;
            } else {
                std::cout << "    => [FAILED] Ma tran Unsolvable (Khong the giai)." << std::endl;
                // Xuất file .sol an toàn để tránh crash game PopCap khi thầy cô bấm Hint
                exportSOLFile(outputSOL, std::vector<Move>());
            }
        }
    }

    std::cout << "\n===============================================================" << std::endl;
    std::cout << "   HOAN THANH QUET: Da xu ly " << successCount << "/" << totalCount << " file .bpz thanh cong!" << std::endl;
    std::cout << "===============================================================" << std::endl;

    return 0;
}