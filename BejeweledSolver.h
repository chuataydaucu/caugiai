#ifndef BEJEWELED_SOLVER_H
#define BEJEWELED_SOLVER_H

#include <iostream>
#include <fstream>
#include <cstdint>
#include <string>
#include <vector>  // Thư viện mảng động chứa chuỗi lời giải

// 1. Định nghĩa các loại ô có thể xuất hiện trên bàn cờ
enum GemType {
    EMPTY,       // Ô trống (Mã byte là 0xFF)
    NORMAL,      // Viên kim cương bình thường
    ROCK,        // Ô Đá cản trở
    BOMB         // Ô Quả bom đếm ngược
};

// 2. Định nghĩa cấu trúc dữ liệu cho từng ô nhỏ
struct Gem {
    GemType type = EMPTY;      // Mặc định ban đầu là ô trống
    uint8_t skin = 0xFF;       // Mã màu/ skin từ 0x00 đến 0x09
    uint8_t isPowered = 0x00;  // 01 là kim cương có năng lượng nổ, 00 là không
    uint8_t bombCounter = 0;   // Lượng giây đếm ngược (chỉ dùng nếu là ô BOMB)
};

// 3. Định nghĩa cấu trúc cho toàn bộ bàn cờ 8x8
struct BejeweledBoard {
    uint8_t modeOffset04 = 0;  // Lưu byte cấu hình ở vị trí thứ 5 (offset 0x04) để phân biệt Đá/Bom
    Gem matrix[8][8];          // Mảng 2 chiều 8 hàng, 8 cột chứa các viên Gem
};

// 4. Cấu trúc lưu trữ thông tin một bước đi của người chơi
struct Move {
    int r1, c1; // Vị trí ô gốc
    int r2, c2; // Vị trí ô hoán đổi kề cạnh
};

// ==================== NGUYÊN MẪU CÁC HÀM XỬ LÝ (PROTOTYPES) ====================

// Hàm đọc file bản đồ nhị phân .bpz
bool loadBPZFile(const std::string& filename, BejeweledBoard& board);

// Hàm in bàn cờ ra màn hình console để kiểm tra đầu vào
void printBoard(const BejeweledBoard& board);

// Các hàm xử lý quy tắc logic vật lý trong game
bool isValidSwap(BejeweledBoard& board, int r1, int c1, int r2, int c2);
bool checkMatchesExist(const BejeweledBoard& board);
void executeMatch(BejeweledBoard& board);
void applyGravity(BejeweledBoard& board);

// Hàm kiểm tra điều kiện chiến thắng sạch bảng
bool isBoardCleared(const BejeweledBoard& board);

// Thuật toán đệ quy quay lui tìm kiếm lời giải phá đảo
bool solvePuzzle(BejeweledBoard& board, std::vector<Move>& solutionPath);

// Hàm đóng gói chuỗi các bước đi và ghi ra file kết quả nhị phân .sol
bool exportSOLFile(const std::string& filename, const std::vector<Move>& solutionPath);

#endif // <-- TỪ KHÓA BẮT BUỘC NÀY PHẢI NẰM Ở CUỐI CÙNG CỦA FILE HEADER