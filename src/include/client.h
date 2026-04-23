#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <utility>
#include <vector>
#include <queue>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <cstring>

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

            // If number of marked around equals the number, all unknown neighbors are safe - auto-explore
            if (marked_around == k && unknown_around > 0) {
                out_r = i;
                out_c = j;
                out_type = 2;  // AutoExplore
                return true;
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

// Structure for a constraint: a set of variables (indices into frontier array) and required mines
struct Constraint {
    std::vector<int> vars;
    int required;
};

int total_solutions;
int mine_count[100];
bool current_assignment[100];

void backtrack(int var_idx, int mines_placed, int total_vars, const std::vector<Constraint> &constraints) {
    // Prune if we can't satisfy constraints
    if (mines_placed > mines_remaining) return;
    if (mines_placed + (total_vars - var_idx) < mines_remaining) return;

    if (var_idx == total_vars) {
        // Check all constraints
        for (const Constraint &c : constraints) {
            int cnt = 0;
            for (int v : c.vars) {
                if (current_assignment[v]) cnt++;
            }
            if (cnt != c.required) return;
        }
        // Valid solution found
        total_solutions++;
        for (int i = 0; i < total_vars; i++) {
            if (current_assignment[i]) mine_count[i]++;
        }
        return;
    }

    // Try not placing mine
    current_assignment[var_idx] = false;
    backtrack(var_idx + 1, mines_placed, total_vars, constraints);

    // Try placing mine, only if we haven't exceeded total
    if (mines_placed < mines_remaining) {
        current_assignment[var_idx] = true;
        backtrack(var_idx + 1, mines_placed + 1, total_vars, constraints);
    }
}

/**
 * Compute mine probability using full constraint satisfaction
 */
void find_best_guess(int &out_r, int &out_c, int &out_type) {
    // Step 1: Collect all frontier cells (unknown adjacent to opened)
    std::vector<std::pair<int, int>> frontier;
    int cell_index[30][30];
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
            cell_index[i][j] = -1;
        }
    }

    bool connected[30][30] = {false};
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
            if (client_grid[i][j] == -2) {
                // Check if adjacent to any opened cell
                bool is_frontier = false;
                for (int d = 0; d < 8; d++) {
                    int ni = i + client_dx[d];
                    int nj = j + client_dy[d];
                    if (ni >= 0 && ni < rows && nj >= 0 && nj < columns && client_grid[ni][nj] >= 0) {
                        is_frontier = true;
                        break;
                    }
                }
                if (is_frontier) {
                    cell_index[i][j] = frontier.size();
                    frontier.push_back({i, j});
                }
            }
        }
    }

    // If no frontier, just pick any unknown
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

    // If frontier is too large, fall back to simple heuristic to avoid exponential blowup
    int n = frontier.size();
    if (n > 25) {
        // Use simple probability method
        double mine_prob[30][30];
        bool in_frontier[30][30] = {false};
        double base_p = (double)mines_remaining / unknown_count;
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < columns; j++) {
                if (client_grid[i][j] == -2) {
                    mine_prob[i][j] = base_p;
                    in_frontier[i][j] = false;
                }
            }
        }
        for (auto &cell : frontier) {
            in_frontier[cell.first][cell.second] = true;
            mine_prob[cell.first][cell.second] = 0;
        }
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
                for (auto &cell : neighbors_unknown) {
                    if (mine_prob[cell.first][cell.second] < p || (in_frontier[cell.first][cell.second] && mine_prob[cell.first][cell.second] == 0)) {
                        mine_prob[cell.first][cell.second] = p;
                    }
                }
            }
        }
        // Find cell with minimum probability, tie-break by fewer unknown neighbors (more constrained)
        double min_p = 2.0;
        int min_unknown = 9;
        out_r = 0;
        out_c = 0;
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < columns; j++) {
                if (client_grid[i][j] != -2) continue;
                if (mine_prob[i][j] < min_p) {
                    int unknown_adj = 0;
                    for (int d = 0; d < 8; d++) {
                        int ni = i + client_dx[d];
                        int nj = j + client_dy[d];
                        if (ni >= 0 && ni < rows && nj >= 0 && nj < columns && client_grid[ni][nj] == -2) {
                            unknown_adj++;
                        }
                    }
                    min_p = mine_prob[i][j];
                    min_unknown = unknown_adj;
                    out_r = i;
                    out_c = j;
                } else if (mine_prob[i][j] == min_p) {
                    int unknown_adj = 0;
                    for (int d = 0; d < 8; d++) {
                        int ni = i + client_dx[d];
                        int nj = j + client_dy[d];
                        if (ni >= 0 && ni < rows && nj >= 0 && nj < columns && client_grid[ni][nj] == -2) {
                            unknown_adj++;
                        }
                    }
                    if (unknown_adj < min_unknown) {
                        min_unknown = unknown_adj;
                        out_r = i;
                        out_c = j;
                    }
                }
            }
        }
        if (mine_prob[out_r][out_c] >= 1.0 - 1e-9) {
            out_type = 1;
        } else {
            out_type = 0;
        }
        return;
    }

    // Step 2: Collect all constraints
    std::vector<Constraint> constraints;
    bool visited[30][30] = {false};
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
            if (client_grid[i][j] < 0) continue;
            int k = client_grid[i][j];
            int marked_around = 0;
            Constraint c;
            for (int d = 0; d < 8; d++) {
                int ni = i + client_dx[d];
                int nj = j + client_dy[d];
                if (ni < 0 || ni >= rows || nj < 0 || nj >= columns) continue;
                if (client_grid[ni][nj] == -1 || mine_marked[ni][nj]) {
                    marked_around++;
                } else if (client_grid[ni][nj] == -2 && cell_index[ni][nj] >= 0) {
                    c.vars.push_back(cell_index[ni][nj]);
                }
            }
            c.required = k - marked_around;
            if (!c.vars.empty()) {
                constraints.push_back(c);
            }
        }
    }

    // Step 3: Backtrack to count solutions and mine frequencies
    total_solutions = 0;
    memset(mine_count, 0, sizeof(mine_count));
    memset(current_assignment, 0, sizeof(current_assignment));
    backtrack(0, 0, n, constraints);

    // If no solutions found (shouldn't happen), fall back to base probability
    if (total_solutions == 0) {
        double base_p = (double)mines_remaining / unknown_count;
        double min_p = base_p;
        int min_unknown = 9;
        out_r = frontier[0].first;
        out_c = frontier[0].second;
        for (auto &cell : frontier) {
            int i = cell.first;
            int j = cell.second;
            int unknown_adj = 0;
            for (int d = 0; d < 8; d++) {
                int ni = i + client_dx[d];
                int nj = j + client_dy[d];
                if (ni >= 0 && ni < rows && nj >= 0 && nj < columns && client_grid[ni][nj] == -2) {
                    unknown_adj++;
                }
            }
            if (base_p < min_p || (base_p == min_p && unknown_adj < min_unknown)) {
                min_p = base_p;
                min_unknown = unknown_adj;
                out_r = i;
                out_c = j;
            }
        }
    } else {
        // Find cell with minimum probability of being mine
        double min_prob = 1.0;
        int min_unknown = 9;
        out_r = frontier[0].first;
        out_c = frontier[0].second;

        for (int i = 0; i < n; i++) {
            double prob = (double)mine_count[i] / total_solutions;
            int r = frontier[i].first;
            int c = frontier[i].second;

            // Count unknown adjacent for tie-breaking
            int unknown_adj = 0;
            for (int d = 0; d < 8; d++) {
                int nr = r + client_dx[d];
                int nc = c + client_dy[d];
                if (nr >= 0 && nr < rows && nc >= 0 && nc < columns && client_grid[nr][nc] == -2) {
                    unknown_adj++;
                }
            }

            if (prob < min_prob || (prob == min_prob && unknown_adj < min_unknown)) {
                min_prob = prob;
                min_unknown = unknown_adj;
                out_r = r;
                out_c = c;
            }
        }

        if (min_prob >= 1.0 - 1e-9) {
            out_type = 1;
        } else {
            out_type = 0;
        }
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

    // No obvious moves, use full constraint-based probability guessing
    find_best_guess(r, c, type);
    Execute(r, c, type);
}

#endif
