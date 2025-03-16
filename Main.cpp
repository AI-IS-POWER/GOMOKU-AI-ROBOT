#include <iostream>
#include <vector>
#include <algorithm>
#include <limits>
#include <windows.h>
#include <chrono>
#include <unordered_map>

using namespace std;

// Game constants
const int BOARD_SIZE = 15;
const char HUMAN_PIECE = 'X';
const char AI_PIECE = 'O';
const char EMPTY_CELL = ' ';

// Advanced pattern weights
const int WIN_SCORE = 1000000;
const int FOUR_SCORE = 100000;
const int BROKEN_FOUR_SCORE = 10000;
const int THREE_SCORE = 1000;
const int BROKEN_THREE_SCORE = 100;

// Zobrist hashing
unsigned long long zobristTable[BOARD_SIZE][BOARD_SIZE][2];
unordered_map<unsigned long long, int> transpositionTable;

// Console colors
enum ConsoleColor {
    COLOR_BLACK = 0,
    COLOR_BLUE = 1,
    COLOR_GREEN = 2,
    COLOR_CYAN = 3,
    COLOR_RED = 4,
    COLOR_MAGENTA = 5,
    COLOR_YELLOW = 6,
    COLOR_WHITE = 7
};

vector<vector<char>> board(BOARD_SIZE, vector<char>(BOARD_SIZE, EMPTY_CELL));
int moveCount = 0;
HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
bool humanFirst = true;
unsigned long long currentHash = 0;

void initializeZobrist() {
    random_device rd;
    mt19937_64 gen(rd());
    uniform_int_distribution<unsigned long long> dis;
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            zobristTable[i][j][0] = dis(gen);
            zobristTable[i][j][1] = dis(gen);
        }
    }
}

void updateHash(int y, int x, char player) {
    int idx = (player == AI_PIECE) ? 1 : 0;
    currentHash ^= zobristTable[y][x][idx];
}

void setColor(int fg, int bg = COLOR_BLACK) {
    SetConsoleTextAttribute(hConsole, (bg << 4) | fg);
}

void drawChar(int x, int y, char c, int color) {
    COORD pos = {static_cast<SHORT>(x), static_cast<SHORT>(y)};
    SetConsoleCursorPosition(hConsole, pos);
    setColor(color);
    cout << c;
}

// ... (drawBoard and checkWin functions remain similar to previous version)

vector<pair<int, int>> getNearbyMoves() {
    vector<pair<int, int>> moves;
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (board[y][x] != EMPTY_CELL) continue;
            
            bool found = false;
            for (int dy = -1; dy <= 1 && !found; dy++) {
                for (int dx = -1; dx <= 1 && !found; dx++) {
                    if (dy == 0 && dx == 0) continue;
                    int ny = y + dy;
                    int nx = x + dx;
                    if (ny >= 0 && ny < BOARD_SIZE && nx >= 0 && nx < BOARD_SIZE) {
                        if (board[ny][nx] != EMPTY_CELL) {
                            moves.emplace_back(y, x);
                            found = true;
                        }
                    }
                }
            }
        }
    }
    return moves.empty() ? vector<pair<int, int>>{make_pair(BOARD_SIZE/2, BOARD_SIZE/2)} : moves;
}

int evaluateDirection(int y, int x, int dy, int dx, char player) {
    int score = 0;
    int consecutive = 0;
    int openEnds = 0;
    
    // Check positive direction
    int i = 1;
    while (true) {
        int cy = y + i*dy;
        int cx = x + i*dx;
        if (cy < 0 || cy >= BOARD_SIZE || cx < 0 || cx >= BOARD_SIZE) break;
        if (board[cy][cx] == player) consecutive++;
        else {
            if (board[cy][cx] == EMPTY_CELL) openEnds++;
            break;
        }
        i++;
    }
    
    // Check negative direction
    i = 1;
    while (true) {
        int cy = y - i*dy;
        int cx = x - i*dx;
        if (cy < 0 || cy >= BOARD_SIZE || cx < 0 || cx >= BOARD_SIZE) break;
        if (board[cy][cx] == player) consecutive++;
        else {
            if (board[cy][cx] == EMPTY_CELL) openEnds++;
            break;
        }
        i++;
    }
    
    consecutive += 1; // Include current cell
    
    if (consecutive >= 5) return WIN_SCORE;
    if (consecutive == 4) return openEnds == 2 ? FOUR_SCORE : BROKEN_FOUR_SCORE;
    if (consecutive == 3) return openEnds == 2 ? THREE_SCORE : BROKEN_THREE_SCORE;
    return 0;
}

int evaluatePosition(int y, int x, char player) {
    int score = 0;
    int directions[4][2] = {{0,1}, {1,0}, {1,1}, {1,-1}};
    
    for (auto& dir : directions) {
        score += evaluateDirection(y, x, dir[0], dir[1], player);
    }
    return score;
}

int evaluateGameState() {
    if (transpositionTable.find(currentHash) != transpositionTable.end()) {
        return transpositionTable[currentHash];
    }
    
    int aiScore = 0;
    int humanScore = 0;
    
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            if (board[y][x] == AI_PIECE) {
                aiScore += evaluatePosition(y, x, AI_PIECE);
            } else if (board[y][x] == HUMAN_PIECE) {
                humanScore += evaluatePosition(y, x, HUMAN_PIECE);
            }
        }
    }
    
    int finalScore = aiScore - humanScore * 2;
    transpositionTable[currentHash] = finalScore;
    return finalScore;
}

int negamax(int depth, int alpha, int beta, char player) {
    if (depth == 0) return evaluateGameState();
    
    vector<pair<int, int>> moves = getNearbyMoves();
    if (moves.empty()) return 0;
    
    // Move ordering with killer heuristic
    sort(moves.begin(), moves.end(), [&](const pair<int, int>& a, const pair<int, int>& b) {
        return evaluatePosition(a.first, a.second, AI_PIECE) > evaluatePosition(b.first, b.second, AI_PIECE);
    });
    
    int bestValue = numeric_limits<int>::min();
    for (auto& move : moves) {
        int y = move.first;
        int x = move.second;
        
        // Make move
        board[y][x] = player;
        updateHash(y, x, player);
        
        // Null move pruning
        if (depth >= 3) {
            int nullScore = -negamax(depth-1-2, -beta, -beta+1, player == AI_PIECE ? HUMAN_PIECE : AI_PIECE);
            if (nullScore >= beta) {
                board[y][x] = EMPTY_CELL;
                updateHash(y, x, player);
                return beta;
            }
        }
        
        int value = -negamax(depth-1, -beta, -alpha, player == AI_PIECE ? HUMAN_PIECE : AI_PIECE);
        
        // Undo move
        board[y][x] = EMPTY_CELL;
        updateHash(y, x, player);
        
        if (value > bestValue) {
            bestValue = value;
            if (value > alpha) alpha = value;
            if (alpha >= beta) break;
        }
    }
    return bestValue;
}

pair<int, int> findBestMove() {
    int bestValue = numeric_limits<int>::min();
    pair<int, int> bestMove = make_pair(BOARD_SIZE/2, BOARD_SIZE/2);
    int depth = 7; // Fixed depth with optimizations
    
    vector<pair<int, int>> moves = getNearbyMoves();
    for (auto& move : moves) {
        int y = move.first;
        int x = move.second;
        
        board[y][x] = AI_PIECE;
        updateHash(y, x, AI_PIECE);
        
        int moveValue = -negamax(depth, numeric_limits<int>::min(), numeric_limits<int>::max(), HUMAN_PIECE);
        
        board[y][x] = EMPTY_CELL;
        updateHash(y, x, AI_PIECE);
        
        if (moveValue > bestValue) {
            bestValue = moveValue;
            bestMove = move;
        }
        
        if (moveValue == WIN_SCORE) break; // Immediate win found
    }
    return bestMove;
}

// ... (main function remains similar with hash initialization)

int main() {
    initializeZobrist();
    // ... rest of main function
}
