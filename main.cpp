#include "BejeweledSolver.h"
#include <vector>
#include <unordered_set> // <-- THÊM THƯ VIỆN BẢNG BĂM NÀY
#include <sstream>       // <-- THÊM THƯ VIỆN CHUYỂN ĐỔI CHUỖI
#include <filesystem> // <-- Thư viện này giúp C++ tự lục tìm file trong máy tính

// HÀM ĐỌC FILE .BPZ NHỊ PHÂN
bool loadBPZFile(const std::string& filename, BejeweledBoard& board) {
    // Mở file ở chế độ ios::binary (đọc nhị phân, đọc từng byte nguyên bản chứ không đọc dạng văn bản text)
    std::ifstream file(filename, std::ios::binary);
    
    if (!file.is_open()) {
        std::cerr << "Loi: Khong the mo duoc file: " << filename << ". Hay kiem tra xem file co nam dung thu muc khong!" << std::endl;
        return false;
    }

    // Đọc 5 byte đầu tiên (Header của file)
    uint8_t header[5];
    file.read(reinterpret_cast<char*>(header), 5);
    
    // Lưu byte ở offset 0x04 (byte thứ 5) để làm căn cứ phân biệt Đá và Bom
    board.modeOffset04 = header[4]; 

    // Duyệt qua ma trận 8 hàng (r) và 8 cột (c) để nạp dữ liệu
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            
            uint8_t firstByte;
            file.read(reinterpret_cast<char*>(&firstByte), 1); // Đọc trước 1 byte đầu tiên của ô này
            
            if (firstByte == 0xFF) { 
                // Cấu trúc loại 1: Nếu gặp 0xFF -> Đây là ô trống, chỉ tốn đúng 1 byte này.
                board.matrix[r][c].type = EMPTY;
                board.matrix[r][c].skin = 0xFF;
            } 
            else {
                // Nếu không phải 0xFF, ta giữ byte này làm mã màu (skin) và xét xem nó là loại gì
                uint8_t currentSkin = firstByte;
                bool isRock = false;
                bool isBomb = false;

                // Quy tắc phân biệt dựa vào byte offset 0x04 đã đọc ở trên
                if (board.modeOffset04 == 0x01) {
                    if (currentSkin == 0x07) isRock = true;
                    if (currentSkin == 0x08) isBomb = true;
                } else if (board.modeOffset04 == 0x02) {
                    if (currentSkin == 0x08) isBomb = true;
                    if (currentSkin == 0x07) isRock = true;
                }

                if (isBomb) {
                    // Cấu trúc loại 2: Quả bom -> Chiếm 3 byte. Ta cần đọc thêm 2 byte tiếp theo nữa.
                    board.matrix[r][c].type = BOMB;
                    board.matrix[r][c].skin = currentSkin;
                    
                    uint8_t buffer[2];
                    file.read(reinterpret_cast<char*>(buffer), 2); // Đọc byte trạng thái và byte đếm ngược
                    board.matrix[r][c].isPowered = buffer[0];
                    board.matrix[r][c].bombCounter = buffer[1];
                } 
                else if (isRock) {
                    // Cấu trúc loại 3: Viên đá -> Chiếm 2 byte. Đọc thêm 1 byte tiếp theo.
                    board.matrix[r][c].type = ROCK;
                    board.matrix[r][c].skin = currentSkin;
                    
                    uint8_t nextByte;
                    file.read(reinterpret_cast<char*>(&nextByte), 1);
                    board.matrix[r][c].isPowered = nextByte;
                } 
                else {
                    // Cấu trúc loại 4: Kim cương thường -> Chiếm 2 byte. Đọc thêm 1 byte tiếp theo.
                    board.matrix[r][c].type = NORMAL;
                    board.matrix[r][c].skin = currentSkin;
                    
                    uint8_t nextByte;
                    file.read(reinterpret_cast<char*>(&nextByte), 1);
                    board.matrix[r][c].isPowered = nextByte;
                }
            }
        }
    }

    file.close(); // Đọc xong thì đóng file lại
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
bool isMatchAt(const BejeweledBoard& board, int r, int c) {
    if (board.matrix[r][c].type == EMPTY || board.matrix[r][c].type == ROCK) return false;
    uint8_t color = board.matrix[r][c].skin;

    // Kiểm tra hàng ngang (3 viên liên tiếp trùng màu)
    if (c >= 2 && board.matrix[r][c-1].skin == color && board.matrix[r][c-2].skin == color) return true;
    if (c <= 5 && board.matrix[r][c+1].skin == color && board.matrix[r][c+2].skin == color) return true;
    if (c >= 1 && c <= 6 && board.matrix[r][c-1].skin == color && board.matrix[r][c+1].skin == color) return true;

    // Kiểm tra hàng dọc (3 viên liên tiếp trùng màu)
    if (r >= 2 && board.matrix[r-1][c].skin == color && board.matrix[r-2][c].skin == color) return true;
    if (r <= 5 && board.matrix[r+1][c].skin == color && board.matrix[r+2][c].skin == color) return true;
    if (r >= 1 && r <= 6 && board.matrix[r-1][c].skin == color && board.matrix[r+1][c].skin == color) return true;

    return false;
}

// 2. Hàm kiểm tra lượt tráo đổi giữa (r1, c1) và (r2, c2) có hợp lệ không
bool isValidSwap(BejeweledBoard& board, int r1, int c1, int r2, int c2) {
    // Chỉ được tráo đổi ô kề cạnh (ngang hoặc dọc)
    if (abs(r1 - r2) + abs(c1 - c2) != 1) return false;
    
    // Đá (ROCK) hoặc ô trống (EMPTY) thì không thể hoán đổi chủ động
    if (board.matrix[r1][c1].type == ROCK || board.matrix[r1][c1].type == EMPTY) return false;
    if (board.matrix[r2][c2].type == ROCK || board.matrix[r2][c2].type == EMPTY) return false;

    // Thử tráo đổi tạm thời
    std::swap(board.matrix[r1][c1], board.matrix[r2][c2]);

    // Kiểm tra xem sau khi tráo, 1 trong 2 ô có tạo ra tổ hợp nổ không
    bool valid = isMatchAt(board, r1, c1) || isMatchAt(board, r2, c2);

    // Tráo ngược trả lại trạng thái cũ để giữ nguyên ma trận ban đầu
    std::swap(board.matrix[r1][c1], board.matrix[r2][c2]);

    return valid;
}

// 3. Hàm thực hiện quét và xóa các tổ hợp Match-3 trở lên
void executeMatch(BejeweledBoard& board) {
    // Mảng đánh dấu các ô sẽ bị nổ xóa
    bool toRemove[8][8] = {false};

    // Quét hàng ngang tìm chuỗi trùng màu >= 3
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 6; ++c) {
            if (board.matrix[r][c].type != EMPTY && board.matrix[r][c].type != ROCK) {
                uint8_t color = board.matrix[r][c].skin;
                if (board.matrix[r][c+1].skin == color && board.matrix[r][c+2].skin == color) {
                    toRemove[r][c] = true;
                    toRemove[r][c+1] = true;
                    toRemove[r][c+2] = true;
                }
            }
        }
    }

    // Quét hàng dọc tìm chuỗi trùng màu >= 3
    for (int c = 0; c < 8; ++c) {
        for (int r = 0; r < 6; ++r) {
            if (board.matrix[r][c].type != EMPTY && board.matrix[r][c].type != ROCK) {
                uint8_t color = board.matrix[r][c].skin;
                if (board.matrix[r+1][c].skin == color && board.matrix[r+2][c].skin == color) {
                    toRemove[r][c] = true;
                    toRemove[r+1][c] = true;
                    toRemove[r+2][c] = true;
                }
            }
        }
    }

    // Tiến hành xóa (biến thành EMPTY) các ô đã đánh dấu nổ
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (toRemove[r][c]) {
                board.matrix[r][c].type = EMPTY;
                board.matrix[r][c].skin = 0xFF;
                board.matrix[r][c].isPowered = 0x00;
            }
        }
    }
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
bool solvePuzzleOptimized(BejeweledBoard& board, std::vector<Move>& solutionPath, std::unordered_set<std::string>& visitedStates) {
    // 1. ĐIỀU KIỆN DỪNG: Nếu bảng sạch hoàn toàn -> Thắng
    if (isBoardCleared(board)) {
        return true;
    }

    // 2. TỐI ƯU BẢNG BĂM: Kiểm tra xem trạng thái ma trận này đã từng tính toán thất bại chưa
    std::string boardKey = serializeBoard(board);
    if (visitedStates.count(boardKey) > 0) {
        return false; // Đã từng duyệt ma trận này rồi và không giải được -> Cắt nhánh, thoát ngay!
    }

    // Duyệt qua ma trận quét nước đi
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            int nextRows[] = {r, r + 1};
            int nextCols[] = {c + 1, c};

            for (int i = 0; i < 2; ++i) {
                int nr = nextRows[i];
                int nc = nextCols[i];

                if (nr < 8 && nc < 8) {
                    if (isValidSwap(board, r, c, nr, nc)) {
                        
                        BejeweledBoard backupBoard = board; 

                        // Thực hiện tráo và mô phỏng phản ứng nổ + rơi
                        std::swap(board.matrix[r][c], board.matrix[nr][nc]);
                        executeMatch(board);
                        applyGravity(board);

                        Move currentMove = {r, c, nr, nc};
                        solutionPath.push_back(currentMove);

                        // Gọi đệ quy tiếp tục giải trạng thái mới
                        if (solvePuzzleOptimized(board, solutionPath, visitedStates)) {
                            return true; 
                        }

                        // Quay lui nếu nhánh này tắc
                        solutionPath.pop_back();
                        board = backupBoard;     
                    }
                }
            }
        }
    }

    // 3. GHI NHỚ VÀO BẢNG BĂM: Nếu đã thử hết mọi nước từ ma trận này mà không thắng, 
    // ta nạp nó vào danh sách đen để lần sau gặp lại không mất công tính lại nữa.
    visitedStates.insert(boardKey);
    return false; 
}
// HÀM XUẤT FILE LỜI GIẢI NHỊ PHÂN .SOL
bool exportSOLFile(const std::string& filename, const std::vector<Move>& solutionPath) {
    // Mở file ghi ở chế độ nhị phân (ios::binary)
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Loi: Khong the tao duoc file output .sol!" << std::endl;
        return false;
    }

    // 1. GHI HEADER: 4 byte định danh đầu tiên của file .sol chuẩn PopCap
    uint8_t solHeader[4] = { 0x00, 0x00, 0x00, 0x00 };
    file.write(reinterpret_cast<char*>(solHeader), 4);

    // Tính toán số lượng trạng thái (States) cần ghi
    // Nếu có N bước đi, ta sẽ tạo ra N + 1 trạng thái (bao gồm cả trạng thái State 0 - Sạch bảng)
    uint16_t totalStates = solutionPath.size() + 1;

    // Duyệt qua từng trạng thái để ghi vào file, viết ngược từ State 0 (đích) lên State xuất phát
    for (int i = 0; i < totalStates; ++i) {
        if (i == 0) {
            // TRẠNG THÁI 0: Đây là trạng thái đích (Sạch bảng hoàn toàn)
            // Tại đây không còn viên kim cương nào cả, số lượng mũi tên gợi ý (Hint) = 0
            uint8_t numHints = 0;
            file.write(reinterpret_cast<char*>(&numHints), 1);
        } 
        else {
            // CÁC TRẠNG THÁI TIẾP THEO (State 1, State 2... tương ứng với từng bước đi)
            // Đề bài chỉ yêu cầu chỉ ra ít nhất 1 cách giải, nên tại mỗi trạng thái ta chỉ cần ghi 1 mũi tên gợi ý duy nhất
            uint8_t numHints = 1; 
            file.write(reinterpret_cast<char*>(&numHints), 1);

            // Lấy thông tin bước đi tương ứng từ chuỗi lời giải (lấy từ cuối lên đầu)
            const Move& currentMove = solutionPath[solutionPath.size() - i];

            // Xác định Hướng gạt (Direction byte): 
            // Căn cứ theo tài liệu hướng dẫn: Phải = 0x00, Trái = 0x01, Dưới = 0x02, Trên = 0x03
            uint8_t direction = 0x00; 
            if (currentMove.c2 > currentMove.c1) direction = 0x00; // Vuốt sang phải
            else if (currentMove.c2 < currentMove.c1) direction = 0x01; // Vuốt sang trái
            else if (currentMove.r2 > currentMove.r1) direction = 0x02; // Vuốt xuống dưới
            else if (currentMove.r2 < currentMove.r1) direction = 0x03; // Vuốt lên trên

            // Quy đổi vị trí ô cờ (Hàng, Cột) ra ID chỉ số ô chạy từ 0 đến 63 của game PopCap
            // Công thức: index = Hàng * 8 + Cột
            uint8_t gemIndex = currentMove.r1 * 8 + currentMove.c1;

            // RÀNG BUỘC CHỐNG CRASH GAME (Cực kỳ quan trọng): 
            // Tài liệu lưu ý mũi tên gợi ý đầu tiên bắt buộc phải trỏ vào 1 viên kim cương có thật chứ không được chỉ vào ô trống.
            // Đoạn code này đảm bảo gemIndex luôn là tọa độ ô gốc hợp lệ trước khi vuốt.
            file.write(reinterpret_cast<char*>(&gemIndex), 1);
            file.write(reinterpret_cast<char*>(&direction), 1);

            // Ghi ID của trạng thái tiếp theo mà nước đi này trỏ tới (Goto State)
            // Bước trước đó sẽ dẫn đến trạng thái liền sau nó (i - 1)
            uint16_t gotoState = i - 1;
            file.write(reinterpret_cast<char*>(&gotoState), 2); // Chiếm 2 byte bộ nhớ
        }
    }

    // 2. GHI PHẦN CUỐI FILE (FOOTER CỐ ĐỊNH): 2 byte kết thúc bắt buộc, nếu đổi sẽ crash game
    uint8_t solFooter[2] = { 0x01, 0x00 }; 
    file.write(reinterpret_cast<char*>(solFooter), 2);

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

            std::vector<Move> solutionPath;
            std::unordered_set<std::string> visitedStates;

            // Kích hoạt bộ não giải đố tối ưu siêu tốc
            if (solvePuzzleOptimized(myBoard, solutionPath, visitedStates)) {
                std::cout << "    => [SUCCESS] Tim thay loi giai sau " << solutionPath.size() << " buoc!" << std::endl;
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
    std::cout << "   HOAN THANH QUET: Da xu ly " << successCount << "/" << totalCount << " file .bpz tha cong!" << std::endl;
    std::cout << "===============================================================" << std::endl;

    return 0;
}