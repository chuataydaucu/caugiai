#include "BejeweledSolver.h"
#include "PuzzleIDs.h"
#include <vector>
#include <unordered_set> 
#include <unordered_map>
#include <sstream>       // <-- THÊM THƯ VIỆN CHUYỂN ĐỔI CHUỖI
#include <filesystem> // <-- Thư viện này giúp C++ tự lục tìm file trong máy tính
#include <algorithm>
#include <queue>
#include <cmath>
#include <chrono>        // <-- Thêm để hỗ trợ timeout

// Deadline toàn cục: solver phải dừng khi vượt quá thời gian này
std::chrono::steady_clock::time_point g_solveDeadline;

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
                    if (currentSkin == 0x07) isBomb = true;
                    if (currentSkin == 0x08) isRock = true;
                } else if (board.modeOffset04 == 0x02) {
                    if (currentSkin == 0x08) isBomb = true;
                    if (currentSkin == 0x07) isRock = true;
                }

                if (currentSkin == 0x09) {
                    board.matrix[r][c].type = NORMAL;
                    board.matrix[r][c].skin = 0xFF;
                    board.matrix[r][c].isPowered = 0x02;
                    board.matrix[r][c].bombCounter = 0;
                    if (curr < content.size()) curr++;
                }
                else if (isBomb) {
                    board.matrix[r][c].type = BOMB;
                    if (curr + 1 < content.size()) {
                        uint8_t colorByte = static_cast<uint8_t>(content[curr++]);
                        uint8_t countByte = static_cast<uint8_t>(content[curr++]);
                        board.matrix[r][c].skin = colorByte;
                        board.matrix[r][c].isPowered = colorByte;
                        board.matrix[r][c].bombCounter = countByte;
                    }
                }
                else if (isRock) {
                    board.matrix[r][c].type = ROCK;
                    board.matrix[r][c].skin = currentSkin;
                    if (curr < content.size()) curr++;
                }
                else {
                    board.matrix[r][c].type = NORMAL;
                    board.matrix[r][c].skin = currentSkin;
                    if (curr < content.size()) {
                        board.matrix[r][c].isPowered = static_cast<uint8_t>(content[curr++]);
                    }
                    board.matrix[r][c].bombCounter = 0;
                }
            }
        }
    }

    std::cout << "Doc file .bpz thanh cong! modeOffset04 = " << (int)board.modeOffset04 << std::endl;
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
            } else if (board.matrix[r][c].type == NORMAL && board.matrix[r][c].isPowered == 0x01) {
                std::cout << "[P" << (int)board.matrix[r][c].skin << "]   ";
            } else if (board.matrix[r][c].type == NORMAL && board.matrix[r][c].isPowered == 0x02) {
                std::cout << "[H]    ";
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
    if (gem.type == NORMAL && gem.isPowered != 0x02) return gem.skin;
    if (gem.type == BOMB) return gem.isPowered; // byte isPowered lưu màu sắc quả bom
    return 0xFF; // EMPTY hoặc ROCK hoặc HYPERCUBE không thể match
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
bool allowGeneralSlides = false;

bool isValidSwap(BejeweledBoard& board, int r1, int c1, int r2, int c2) {
    // Chỉ được tráo đổi ô kề cạnh (ngang hoặc dọc)
    if (abs(r1 - r2) + abs(c1 - c2) != 1) return false;

    // Không thể hoán đổi hai ô trống (EMPTY)
    if (board.matrix[r1][c1].type == EMPTY && board.matrix[r2][c2].type == EMPTY) return false;

    // Trường hợp hoán đổi có ô EMPTY (trượt ngọc)
    if (board.matrix[r1][c1].type == EMPTY || board.matrix[r2][c2].type == EMPTY) {
        // Xác định ô nào là ngọc, ô nào là trống
        int gemR = (board.matrix[r1][c1].type != EMPTY) ? r1 : r2;
        int gemC = (board.matrix[r1][c1].type != EMPTY) ? c1 : c2;
        int emptyR = (board.matrix[r1][c1].type == EMPTY) ? r1 : r2;
        int emptyC = (board.matrix[r1][c1].type == EMPTY) ? c1 : c2;

        // Đá không thể trượt thủ công vào ô trống
        if (board.matrix[gemR][gemC].type == ROCK) {
            return false;
        }

        // Hypercube không thể trượt vào ô trống để nổ (vì không có màu để kích hoạt)
        if (board.matrix[gemR][gemC].type == NORMAL && board.matrix[gemR][gemC].isPowered == 0x02) {
            return false;
        }

        if (allowGeneralSlides) {
            return true;
        }

        // Trượt ngọc vào ô trống tạm thời
        std::swap(board.matrix[gemR][gemC], board.matrix[emptyR][emptyC]);
        bool valid = isMatchAt(board, emptyR, emptyC);
        std::swap(board.matrix[gemR][gemC], board.matrix[emptyR][emptyC]);
        return valid;
    }


    // Nếu một trong hai viên là Hypercube (isPowered == 0x02)
    bool isHC1 = (board.matrix[r1][c1].type == NORMAL && board.matrix[r1][c1].isPowered == 0x02);
    bool isHC2 = (board.matrix[r2][c2].type == NORMAL && board.matrix[r2][c2].isPowered == 0x02);
    if (isHC1 || isHC2) {
        return true;
    }

    // Thử tráo đổi tạm thời
    std::swap(board.matrix[r1][c1], board.matrix[r2][c2]);

    // Kiểm tra xem sau khi tráo, các ô có tạo ra tổ hợp nổ không
    bool valid = false;
    if (isMatchAt(board, r1, c1)) valid = true;
    if (isMatchAt(board, r2, c2)) valid = true;

    // Tráo ngược trả lại trạng thái cũ
    std::swap(board.matrix[r1][c1], board.matrix[r2][c2]);

    return valid;
}

// Hàm phụ trợ tìm độ dài chuỗi liên tục
bool hasContiguousLine(const std::vector<std::pair<int, int>>& C, bool horizontal, int K) {
    if (horizontal) {
        for (int r = 0; r < 8; ++r) {
            int maxContig = 0;
            int currentContig = 0;
            for (int c = 0; c < 8; ++c) {
                bool found = false;
                for (const auto& p : C) {
                    if (p.first == r && p.second == c) {
                        found = true;
                        break;
                    }
                }
                if (found) {
                    currentContig++;
                    if (currentContig > maxContig) maxContig = currentContig;
                } else {
                    currentContig = 0;
                }
            }
            if (maxContig >= K) return true;
        }
    } else {
        for (int c = 0; c < 8; ++c) {
            int maxContig = 0;
            int currentContig = 0;
            for (int r = 0; r < 8; ++r) {
                bool found = false;
                for (const auto& p : C) {
                    if (p.first == r && p.second == c) {
                        found = true;
                        break;
                    }
                }
                if (found) {
                    currentContig++;
                    if (currentContig > maxContig) maxContig = currentContig;
                } else {
                    currentContig = 0;
                }
            }
            if (maxContig >= K) return true;
        }
    }
    return false;
}

// 3. Hàm thực hiện quét và xóa các tổ hợp Match-3 trở lên
bool executeMatchAndExplosions(BejeweledBoard& board, int swapR1 = -1, int swapC1 = -1, int swapR2 = -1, int swapC2 = -1) {
    bool markedH[8][8] = {false};
    bool markedV[8][8] = {false};
    bool markedAny = false;

    // Quét hàng ngang tìm chuỗi trùng màu >= 3
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 6; ++c) {
            uint8_t color = getGemColor(board, r, c);
            if (color != 0xFF) {
                if (getGemColor(board, r, c+1) == color && getGemColor(board, r, c+2) == color) {
                    markedH[r][c] = true;
                    markedH[r][c+1] = true;
                    markedH[r][c+2] = true;
                    markedAny = true;
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
                    markedV[r][c] = true;
                    markedV[r+1][c] = true;
                    markedV[r+2][c] = true;
                    markedAny = true;
                }
            }
        }
    }

    if (!markedAny) return false;

    bool visited[8][8] = {false};
    bool toRemove[8][8] = {false};

    struct CreatedSpecialGem {
        int r, c;
        uint8_t skin;
        uint8_t isPowered;
    };
    std::vector<CreatedSpecialGem> specialGemsToCreate;

    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if ((markedH[r][c] || markedV[r][c]) && !visited[r][c]) {
                uint8_t color = getGemColor(board, r, c);
                // DFS tìm component C
                std::vector<std::pair<int, int>> C;
                std::vector<std::pair<int, int>> queue;
                queue.push_back({r, c});
                visited[r][c] = true;
                int qIdx = 0;
                while (qIdx < (int)queue.size()) {
                    auto [currR, currC] = queue[qIdx++];
                    C.push_back({currR, currC});
                    int dr[] = {-1, 1, 0, 0};
                    int dc[] = {0, 0, -1, 1};
                    for (int i = 0; i < 4; ++i) {
                        int nr = currR + dr[i];
                        int nc = currC + dc[i];
                        if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8) {
                            if ((markedH[nr][nc] || markedV[nr][nc]) && !visited[nr][nc]) {
                                if (getGemColor(board, nr, nc) == color) {
                                    visited[nr][nc] = true;
                                    queue.push_back({nr, nc});
                                }
                            }
                        }
                    }
                }

                // Kiểm tra loại ngọc đặc biệt tạo ra
                bool hasLine5 = hasContiguousLine(C, true, 5) || hasContiguousLine(C, false, 5);
                bool hasLine4 = hasContiguousLine(C, true, 4) || hasContiguousLine(C, false, 4);
                
                // Kiểm tra xem có giao điểm hay không (L/T shape)
                bool hasIntersection = false;
                for (const auto& p : C) {
                    if (markedH[p.first][p.second] && markedV[p.first][p.second]) {
                        hasIntersection = true;
                        break;
                    }
                }
                
                uint8_t createType = 0x00; // NONE
                // Cả người chơi và cascade đều tạo ngọc đặc biệt trong Bejeweled 2
                if (hasLine5) {
                    createType = 0x02; // HYPERCUBE
                } else if (hasLine4 || hasIntersection) {
                    createType = 0x01; // POWER_GEM
                }

                if (createType != 0x00) {
                    int tr = -1, tc = -1;
                    
                    // Ưu tiên vị trí giao điểm (cho L/T shape hoặc các hình đặc biệt)
                    for (const auto& p : C) {
                        if (markedH[p.first][p.second] && markedV[p.first][p.second]) {
                            tr = p.first;
                            tc = p.second;
                            break;
                        }
                    }
                    
                    // Nếu không có giao điểm, ưu tiên vị trí swap của player
                    if (tr == -1) {
                        for (const auto& p : C) {
                            if ((p.first == swapR1 && p.second == swapC1) || (p.first == swapR2 && p.second == swapC2)) {
                                tr = p.first;
                                tc = p.second;
                                break;
                            }
                        }
                    }

                    // Vị trí đầu tiên
                    if (tr == -1) {
                        tr = C[0].first;
                        tc = C[0].second;
                    }

                    specialGemsToCreate.push_back({tr, tc, color, createType});

                    for (const auto& p : C) {
                        if (p.first != tr || p.second != tc) {
                            toRemove[p.first][p.second] = true;
                        }
                    }
                } else {
                    for (const auto& p : C) {
                        toRemove[p.first][p.second] = true;
                    }
                }
            }
        }
    }

    // Tạo các ngọc đặc biệt trước khi vụ nổ lan truyền xảy ra
    for (const auto& sg : specialGemsToCreate) {
        board.matrix[sg.r][sg.c].type = NORMAL;
        board.matrix[sg.r][sg.c].skin = sg.isPowered == 0x02 ? 0xFF : sg.skin;
        board.matrix[sg.r][sg.c].isPowered = sg.isPowered;
        board.matrix[sg.r][sg.c].bombCounter = 0;
    }

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

    // BẢO VỆ CÁC NGỌC ĐẶC BIỆT VỪA ĐƯỢC TẠO RA
    for (const auto& sg : specialGemsToCreate) {
        toRemove[sg.r][sg.c] = false;
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
bool applyGravity(BejeweledBoard& board) {
    bool boardChanged = false;
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
                if (upperRow >= 0) {
                    board.matrix[r][c] = board.matrix[upperRow][c];
                    board.matrix[upperRow][c].type = EMPTY; // Ô cũ trở thành trống
                    board.matrix[upperRow][c].skin = 0xFF;
                    boardChanged = true;
                }
            }
        }
    }
    return boardChanged;
}
// 1. Hàm kiểm tra điều kiện thắng: Toàn bộ ma trận phải là ô EMPTY (0xFF)
bool isBoardCleared(const BejeweledBoard& board) {
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (board.matrix[r][c].type != EMPTY) {
                return false;
            }
        }
    }
    return true;
} // Bảng sạch bóng 100%


// Hàm chuyển đổi ma trận 8x8 thành một chuỗi String duy nhất để làm khóa băm (Hash Key)
std::string serializeBoard(const BejeweledBoard& board) {
    std::stringstream ss;
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            // Nối mã loại ô và màu sắc, trạng thái năng lượng, đếm ngược bom
            ss << (int)board.matrix[r][c].type << "," 
               << (int)board.matrix[r][c].skin << ","
               << (int)board.matrix[r][c].isPowered << ","
               << (int)board.matrix[r][c].bombCounter << "|";
        }
    }
    return ss.str();
}

// 2. Thuật toán Đệ quy kết hợp Quay lui (Backtracking) để giải bài toán
// HÀM GIẢI BÀI TOÁN NÂNG CẤP TỐI ƯU CẮT NHÁNH CẬN
void resolveBoardStates(BejeweledBoard& board) {
    bool boardChanged = true;
    while (boardChanged) {
        boardChanged = false;
        
        // 1. Tìm và xóa các match
        if (executeMatchAndExplosions(board)) {
            boardChanged = true;
        }
        
        // 2. Áp dụng trọng lực
        if (applyGravity(board)) {
            boardChanged = true;
        }
    }
}

void triggerHypercube(BejeweledBoard& board, int hr, int hc, int tr, int tc) {
    uint8_t targetColor = getGemColor(board, tr, tc);
    bool toRemove[8][8] = {false};
    toRemove[tr][tc] = true; // Xóa ô ngọc hoán đổi
    toRemove[hr][hc] = true; // Xóa bản thân Hypercube

    // Hypercube activation xóa toàn bộ ROCK trên bàn cờ
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (board.matrix[r][c].type == ROCK) {
                toRemove[r][c] = true;
            }
        }
    }

    if (targetColor != 0xFF) {
        for (int r = 0; r < 8; ++r) {
            for (int c = 0; c < 8; ++c) {
                if (getGemColor(board, r, c) == targetColor) {
                    toRemove[r][c] = true;
                }
            }
        }
    } else if (board.matrix[tr][tc].type == NORMAL && board.matrix[tr][tc].isPowered == 0x02) {
        // Hoán đổi 2 Hypercube -> Chỉ xóa tất cả các Hypercube trên bàn cờ
        for (int r = 0; r < 8; ++r) {
            for (int c = 0; c < 8; ++c) {
                if (board.matrix[r][c].type == NORMAL && board.matrix[r][c].isPowered == 0x02) {
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

void triggerBombExplosion(BejeweledBoard& board, int bombR, int bombC) {
    bool toRemove[8][8] = {false};
    toRemove[bombR][bombC] = true;

    std::vector<std::pair<int, int>> explosionQueue;
    bool exploded[8][8] = {false};

    explosionQueue.push_back({bombR, bombC});
    exploded[bombR][bombC] = true;

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
}

bool tickBombs(BejeweledBoard& board) {
    std::vector<std::pair<int, int>> bombsToExplode;
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (board.matrix[r][c].type == BOMB) {
                if (board.matrix[r][c].bombCounter > 0) {
                    board.matrix[r][c].bombCounter--;
                }
                if (board.matrix[r][c].bombCounter == 0) {
                    bombsToExplode.push_back({r, c});
                }
            }
        }
    }

    if (!bombsToExplode.empty()) {
        for (auto [br, bc] : bombsToExplode) {
            if (board.matrix[br][bc].type == BOMB) {
                triggerBombExplosion(board, br, bc);
            }
        }
        applyGravity(board);
        resolveBoardStates(board);
    }
    return true;
}

struct CandidateMove {
    Move m;
    int priority;
};

bool debugSolve = false;

bool solvePuzzleOptimized(BejeweledBoard& board, std::vector<Move>& solutionPath, std::unordered_map<std::string, int>& visitedStates, int maxDepth) {
    // Kiểm tra timeout trước tiên
    if (std::chrono::steady_clock::now() >= g_solveDeadline) {
        return false;
    }

    if (isBoardCleared(board)) {
        return true;
    }

    std::string boardKey = serializeBoard(board);
    int currentDepth = static_cast<int>(solutionPath.size());
    if (currentDepth >= maxDepth) {
        return false;
    }
    
    if (visitedStates.count(boardKey) > 0 && visitedStates[boardKey] <= currentDepth) {
        if (debugSolve) {
            std::cout << "Depth " << currentDepth << ", Path: ";
            for (const auto& m : solutionPath) {
                std::cout << "(" << m.r1 << "," << m.c1 << "->" << m.r2 << "," << m.c2 << ") ";
            }
            std::cout << "\n  -> Pruned! (Visited before at depth " << visitedStates[boardKey] << ")" << std::endl;
            std::cout << "Pruned board state:" << std::endl;
            printBoard(board);
        }
        return false;
    }
    visitedStates[boardKey] = currentDepth;

    if (debugSolve) {
        std::cout << "Depth " << currentDepth << ", Path: ";
        for (const auto& m : solutionPath) {
            std::cout << "(" << m.r1 << "," << m.c1 << "->" << m.r2 << "," << m.c2 << ") ";
        }
        std::cout << std::endl;
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
                            executeMatchAndExplosions(tempBoard, r, c, nr, nc);
                            applyGravity(tempBoard);
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
                        // Đảm bảo tọa độ đầu (r1, c1) luôn luôn chỉ vào một viên ngọc thực tế chứ không chỉ vào ô trống (EMPTY), tránh sập game khi bấm Hint
                        Move m;
                        if (board.matrix[r][c].type == EMPTY) {
                            m = {nr, nc, r, c};
                        } else {
                            m = {r, c, nr, nc};
                        }
                        int priority = rocksDestroyed * 1000 + bombsDestroyed * 500 + emptyCount;
                        candidates.push_back({m, priority});
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
            executeMatchAndExplosions(board, cand.m.r1, cand.m.c1, cand.m.r2, cand.m.c2);
            applyGravity(board);
            resolveBoardStates(board);
        }

        bool bombsOk = tickBombs(board);
        if (bombsOk) {
            solutionPath.push_back(cand.m);
            if (solvePuzzleOptimized(board, solutionPath, visitedStates, maxDepth)) {
                return true;
            }
            solutionPath.pop_back();
        }

        board = backupBoard;
    }

    return false;
}

bool solvePuzzle(BejeweledBoard& board, std::vector<Move>& solutionPath) {
    // Đặt deadline: tối đa 45 giây cho mỗi màn
    g_solveDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);

    // Thử giải bằng IDDFS với strict slides (độ sâu từ 1 đến 15)
    allowGeneralSlides = false;
    for (int limit = 1; limit <= 15; ++limit) {
        if (std::chrono::steady_clock::now() >= g_solveDeadline) break;
        std::unordered_map<std::string, int> visitedStates;
        solutionPath.clear();
        if (solvePuzzleOptimized(board, solutionPath, visitedStates, limit)) {
            return true;
        }
    }

    // Nếu không giải được, thử giải bằng IDDFS với general slides (độ sâu từ 1 đến 15)
    allowGeneralSlides = true;
    for (int limit = 1; limit <= 15; ++limit) {
        if (std::chrono::steady_clock::now() >= g_solveDeadline) break;
        std::unordered_map<std::string, int> visitedStates;
        solutionPath.clear();
        if (solvePuzzleOptimized(board, solutionPath, visitedStates, limit)) {
            return true;
        }
    }

    return false;
}
// Hàm tính CRC32 của một file
uint32_t calculateFileCRC32(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return 0; // Trả về 0 nếu không mở được file
    }
    
    // Đọc toàn bộ nội dung file
    std::vector<char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // Tính toán CRC32
    uint32_t crc = 0xFFFFFFFFU;
    for (char c : buffer) {
        uint8_t byte = static_cast<uint8_t>(c);
        crc ^= byte;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320U;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
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

    // Lấy tên file .bpz tương ứng từ tên file .sol để tính CRC32
    std::string bpzFilename = filename;
    size_t dotPos = bpzFilename.find_last_of('.');
    if (dotPos != std::string::npos) {
        bpzFilename = bpzFilename.substr(0, dotPos) + ".bpz";
    }

    // Tính toán CRC32 thực tế của file .bpz để làm Puzzle ID
    uint32_t puzzleID = calculateFileCRC32(bpzFilename);
    if (puzzleID == 0) {
        // Fallback về ID chính thức nếu không tính được CRC32 trực tiếp
        std::string bpzNameOnly = bpzFilename;
        size_t slashPos = bpzNameOnly.find_last_of("/\\");
        if (slashPos != std::string::npos) {
            bpzNameOnly = bpzNameOnly.substr(slashPos + 1);
        }
        puzzleID = getOfficialPuzzleID(bpzNameOnly);
    }
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

        // Tính hướng di chuyển theo định dạng chính thức của Bejeweled 2 Deluxe:
        // 0x00: Phải (Right)
        // 0x01: Trên (Up)
        // 0x02: Trái (Left)
        // 0x03: Dưới (Down)
        uint8_t direction = 0x00;
        if (currentMove.c2 > currentMove.c1) {
            direction = 0x00; // Phải (Right)
        } else if (currentMove.r2 < currentMove.r1) {
            direction = 0x01; // Trên (Up)
        } else if (currentMove.c2 < currentMove.c1) {
            direction = 0x02; // Trái (Left)
        } else if (currentMove.r2 > currentMove.r1) {
            direction = 0x03; // Dưới (Down)
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
    std::vector<std::string> failedFiles;

    // Chương trình sẽ quét qua tất cả các file
    for (const auto& entry : std::filesystem::directory_iterator(".")) {
        std::string filePath = entry.path().string();
        
        // Nếu phát hiện thấy file nào có đuôi là .bpz
        if (entry.path().extension() == ".bpz") {
            std::string inputBPZ = entry.path().filename().string();
            totalCount++;
            
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

            // Ready to solve
            debugSolve = false;

            std::vector<Move> solutionPath;
            bool solved = false;
            std::cout << "Board state:" << std::endl;
            printBoard(myBoard);
            
            solved = solvePuzzle(myBoard, solutionPath);
            if (solved) {
                std::cout << "    => [SUCCESS] Tim thay loi giai sau " << solutionPath.size() << " buoc!" << std::endl;
            }

            if (solved) {
                for (size_t i = 0; i < solutionPath.size(); ++i) {
                    std::cout << "       Buoc " << i + 1 << ": Hoan doi (" << solutionPath[i].r1 << ", " << solutionPath[i].c1 
                              << ") voi (" << solutionPath[i].r2 << ", " << solutionPath[i].c2 << ")" << std::endl;
                }
                exportSOLFile(outputSOL, solutionPath);
                successCount++;
            } else {
                std::cout << "    => [FAILED] Ma tran Unsolvable (Khong the giai)." << std::endl;
                failedFiles.push_back(inputBPZ);
                // Xuất file .sol an toàn để tránh crash game PopCap khi thầy cô bấm Hint
                exportSOLFile(outputSOL, std::vector<Move>());
            }
        }
    }

    std::cout << "\n===============================================================" << std::endl;
    std::cout << "   HOAN THANH QUET: Da xu ly " << successCount << "/" << totalCount << " file .bpz thanh cong!" << std::endl;
    if (!failedFiles.empty()) {
        std::cout << "   Cac file that bai:" << std::endl;
        for (const auto& file : failedFiles) {
            std::cout << "      - " << file << std::endl;
        }
    }
    std::cout << "===============================================================" << std::endl;

    return 0;
}