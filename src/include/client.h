#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <utility>
#include <vector>
#include <queue>
#include <algorithm>
#include <iomanip>
#include <cmath>

extern int rows;         // The count of rows of the game map. You MUST NOT modify its name.
extern int columns;      // The count of columns of the game map. You MUST NOT modify its name.
extern int total_mines;  // The count of mines of the game map.

void Execute(int r, int c, int type);

// You MUST NOT use any other external variables except for rows, columns and total_mines.

// Cell states known to client
int client_grid[30][30];  // For UNKNOWN: -2, MARKED: -1, OPENED: 0-8
bool mine_marked[30][30]; // Whether we marked this as mine
int unknown_count;        // Number of unknown cells remaining
int mines_remaining;      // Number of mines still to be found

const int client_dx[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
const int client_dy[8] = {-1, 0, 1, -1, 1, -1, 0, 1};

/**
 * @brief The definition of function InitGame()
 *
 * @details This function is designed to initialize the game. It should be called at the beginning of the game, which
 * will read the scale of the game map and the first step taken by the server (see README).
 */
void InitGame() {
    // Initialize all cells to UNKNOWN (-2)
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
            client_grid[i][j] = -2;
            mine_marked[i][j] = false;
        }
    }
    unknown_count = rows * columns;
    mines_remaining = total_mines;

    int first_row, first_column;
    std::cin >> first_row >> first_column;
    Execute(first_row, first_column, 0);
}

/**
 * @brief The definition of function ReadMap()
 *
 * @details This function is designed to read the game map from stdin when playing the client's (or player's) role.
 * Since the client (or player) can only get the limited information of the game map, so if there is a 3 * 3 map as
 * above and only the block (2, 0) has been visited, the stdin would be
 *     ???
 *     12?
 *     01?
 */
void ReadMap() {
    unknown_count = 0;
    for (int i = 0; i < rows; i++) {
        std::string line;
        std::cin >> line;
        for (int j = 0; j < columns; j++) {
            char c = line[j];
            if (c == '?') {
                if (client_grid[i][j] != -1) { // Not marked by us
                    client_grid[i][j] = -2;
                    unknown_count++;
                }
            } else if (c == '@') {
                // We don't mark it in client_grid - it's a mine, but since game isn't over yet, means it's marked by us
                client_grid[i][j] = -1;
                if (!mine_marked[i][j]) {
                    mine_marked[i][j] = true;
                    mines_remaining--;
                }
            } else if (c >= '0' && c <= '8') {
                client_grid[i][j] = c - '0';
                if (mine_marked[i][j]) {
                    mine_marked[i][j] = false;
                    mines_remaining++;
                }
            } else if (c == 'X') {
                // Shouldn't happen - game would have ended
                client_grid[i][j] = -1;
            }
        }
    }

    // Update mines_remaining based on our marks
    mines_remaining = total_mines;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
            if (mine_marked[i][j]) {
                mines_remaining--;
            }
        }
    }
}

/**
 * Check all opened cells for obvious moves - if obvious, mark or open them.
 * Returns true if we found a move to make.
 */
bool find_obvious_move(int &out_r, int &out_c, int &out_type) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
            if (client_grid[i][j] < 0) continue;  // not opened

            int k = client_grid[i][j];
            int unknown_around = 0;
            int marked_around = 0;

            for (int d = 0; d < 8; d++) {
                int ni = i + client_dx[d];
                int nj = j + client_dy[d];
                if (ni < 0 || ni >= rows || nj < 0 || nj >= columns) continue;
                if (client_grid[ni][nj] == -2) unknown_around++;
                if (client_grid[ni][nj] == -1 || mine_marked[ni][nj]) marked_around++;
            }

            // If number of marked around equals the number, all unknown neighbors are safe - open them
            if (marked_around == k && unknown_around > 0) {
                // Find an unknown to open
                for (int d = 0; d < 8; d++) {
                    int ni = i + client_dx[d];
                    int nj = j + client_dy[d];
                    if (ni >= 0 && ni < rows && nj >= 0 && nj < columns && client_grid[ni][nj] == -2) {
                        // AutoExplore on this cell (i,j) - it will automatically open all unknown neighbors
                        out_r = i;
                        out_c = j;
                        out_type = 2;  // AutoExplore
                        return true;
                    }
                }
            }

            // If unknown neighbors equals the remaining mines to find, all must be mines - mark them
            if (marked_around + unknown_around == k && unknown_around > 0) {
                for (int d = 0; d < 8; d++) {
                    int ni = i + client_dx[d];
                    int nj = j + client_dy[d];
                    if (ni >= 0 && ni < rows && nj >= 0 && nj < columns && client_grid[ni][nj] == -2) {
                        out_r = ni;
                        out_c = nj;
                        out_type = 1;  // Mark as mine
                        return true;
                    }
                }
            }
        }
    }

    // Check special case: if remaining unknown cells == remaining mines, all unknown are mines
    if (unknown_count > 0 && unknown_count == mines_remaining && mines_remaining > 0) {
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < columns; j++) {
                if (client_grid[i][j] == -2) {
                    out_r = i;
                    out_c = j;
                    out_type = 1;
                    return true;
                }
            }
        }
    }

    // If no mines remaining, any unknown is safe
    if (mines_remaining == 0 && unknown_count > 0) {
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < columns; j++) {
                if (client_grid[i][j] == -2) {
                    out_r = i;
                    out_c = j;
                    out_type = 0;
                    return true;
                }
            }
        }
    }

    return false;
}

/**
 * Compute mine probability for each unknown cell using the constraint-based approach.
 * Find the cell with lowest probability of being a mine.
 */
void find_best_guess(int &out_r, int &out_c, int &out_type) {
    // Count how many mines each frontier cell must have based on constraints
    std::vector<std::pair<int, int>> frontier;  // unknown cells adjacent to opened cells
    bool in_frontier[30][30] = {false};

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
            if (client_grid[i][j] == -2) {
                // Check if adjacent to any opened cell
                bool adjacent = false;
                for (int d = 0; d < 8; d++) {
                    int ni = i + client_dx[d];
                    int nj = j + client_dy[d];
                    if (ni >= 0 && ni < rows && nj >= 0 && nj < columns && client_grid[ni][nj] >= 0) {
                        adjacent = true;
                        break;
                    }
                }
                if (adjacent) {
                    frontier.push_back({i, j});
                    in_frontier[i][j] = true;
                }
            }
        }
    }

    // If no frontier cell (all unknown isolated), just pick any unknown
    if (frontier.empty()) {
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < columns; j++) {
                if (client_grid[i][j] == -2) {
                    out_r = i;
                    out_c = j;
                    out_type = 0;
                    return;
                }
            }
        }
    }

    // Build probability map - count how many times each frontier cell is forced to be a mine
    double mine_prob[30][30] = {0};
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
            mine_prob[i][j] = 1.0;  // default: full probability for non-frontier
        }
    }

    for (auto &cell : frontier) {
        mine_prob[cell.first][cell.second] = 0;
    }

    // For each opened cell, calculate probability based on the constraint
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
            if (client_grid[i][j] < 0) continue;

            int k = client_grid[i][j];
            std::vector<std::pair<int, int>> neighbors_unknown;
            int marked_around = 0;

            for (int d = 0; d < 8; d++) {
                int ni = i + client_dx[d];
                int nj = j + client_dy[d];
                if (ni < 0 || ni >= rows || nj < 0 || nj >= columns) continue;
                if (client_grid[ni][nj] == -2) {
                    neighbors_unknown.push_back({ni, nj});
                } else if (client_grid[ni][nj] == -1 || mine_marked[ni][nj]) {
                    marked_around++;
                }
            }

            if (neighbors_unknown.empty()) continue;

            int required_mines = k - marked_around;
            if (required_mines <= 0) continue;

            double p = (double)required_mines / neighbors_unknown.size();
            for (auto &n : neighbors_unknown) {
                if (mine_prob[n.first][n.second] < p || mine_prob[n.first][n.second] == 0) {
                    mine_prob[n.first][n.second] = p;
                }
            }
        }
    }

    // For non-frontier unknowns, the base probability is just mines_remaining / unknown_count
    double base_p = (double)mines_remaining / unknown_count;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
            if (client_grid[i][j] == -2 && !in_frontier[i][j]) {
                mine_prob[i][j] = base_p;
            }
        }
    }

    // Find the unknown cell with minimum probability of being a mine
    double min_p = 2.0;
    out_r = 0;
    out_c = 0;

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
            if (client_grid[i][j] == -2 && mine_prob[i][j] < min_p) {
                min_p = mine_prob[i][j];
                out_r = i;
                out_c = j;
            }
        }
    }

    // If probability is 1.0, we should mark it instead of clicking
    if (mine_prob[out_r][out_c] >= 1.0 - 1e-9) {
        out_type = 1;  // mark as mine
    } else {
        out_type = 0;  // click to open
    }
}

/**
 * @brief The definition of function Decide()
 *
 * @details This function is designed to decide the next step when playing the client's (or player's) role. Open up your
 * mind and make your decision here! Caution: you can only execute once in this function.
 */
void Decide() {
    int r, c, type;

    // First check for obvious moves - this gives us 100% certainty
    if (find_obvious_move(r, c, type)) {
        Execute(r, c, type);
        return;
    }

    // No obvious moves, make the best guess based on probability
    find_best_guess(r, c, type);
    Execute(r, c, type);
}

#endif
