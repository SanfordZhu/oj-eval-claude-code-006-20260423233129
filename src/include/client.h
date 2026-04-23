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
extern int total_mines;  // The count of mines of the game map. You MUST NOT modify its name.

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
 * Compute mine probability using full constraint satisfaction on connected components
 */
void find_best_guess(int &out_r, int &out_c, int &out_type) {
    // Initialize probability to base global probability
    double mine_prob[30][30];
    double base_p = (double)mines_remaining / unknown_count;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
            if (client_grid[i][j] == -2) {
                mine_prob[i][j] = base_p;
            }
        }
    }

    // Collect all frontier unknown cells that are adjacent to opened cells
    bool visited[30][30] = {false};
    bool in_frontier[30][30] = {false};

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
            if (client_grid[i][j] == -2 && !visited[i][j]) {
                // Check if this cell is adjacent to any opened cell -> part of frontier
                bool is_frontier = false;
                for (int d = 0; d < 8; d++) {
                    int ni = i + client_dx[d];
                    int nj = j + client_dy[d];
                    if (ni >= 0 && ni < rows && nj >= 0 && nj < columns && client_grid[ni][nj] >= 0) {
                        is_frontier = true;
                        break;
                    }
                }
                if (!is_frontier) continue;

                // BFS to find connected component of frontier
                std::vector<std::pair<int, int>> component;
                std::queue<std::pair<int, int>> q;
                q.push({i, j});
                visited[i][j] = true;
                component.push_back({i, j});

                while (!q.empty()) {
                    auto curr = q.front();
                    q.pop();
                    int x = curr.first;
                    int y = curr.second;
                    // Check 4 directions for connected unknowns that are also frontier
                    for (int d = 0; d < 8; d++) {
                        int nx = x + client_dx[d];
                        int ny = y + client_dy[d];
                        if (nx >= 0 && nx < rows && ny >= 0 && ny < columns && !visited[nx][ny] && client_grid[nx][ny] == -2) {
                            // Check if connected component is frontier (adjacent to opened)
                            bool comp_is_frontier = false;
                            for (int d2 = 0; d2 < 8; d2++) {
                                int n2x = nx + client_dx[d2];
                                int n2y = ny + client_dy[d2];
                                if (n2x >= 0 && n2x < rows && n2y >= 0 && n2y < columns && client_grid[n2x][n2y] >= 0) {
                                    comp_is_frontier = true;
                                    break;
                                }
                            }
                            if (comp_is_frontier) {
                                visited[nx][ny] = true;
                                q.push({nx, ny});
                                component.push_back({nx, ny});
                            }
                        }
                    }
                }

                // If component is small enough, do full constraint solving
                if (component.size() <= 16) {
                    // Map cell to index in component
                    int cell_index[30][30];
                    for (int x = 0; x < rows; x++) {
                        for (int y = 0; y < columns; y++) {
                            cell_index[x][y] = -1;
                        }
                    }
                    for (int idx = 0; idx < component.size(); idx++) {
                        int x = component[idx].first;
                        int y = component[idx].second;
                        cell_index[x][y] = idx;
                        in_frontier[x][y] = true;
                    }

                    // Collect all constraints from opened cells touching this component
                    std::vector<Constraint> constraints;
                    for (int x = 0; x < rows; x++) {
                        for (int y = 0; y < columns; y++) {
                            if (client_grid[x][y] < 0) continue;
                            int k = client_grid[x][y];
                            int marked_around = 0;
                            Constraint c;
                            for (int d = 0; d < 8; d++) {
                                int nx = x + client_dx[d];
                                int ny = y + client_dy[d];
                                if (nx < 0 || nx >= rows || ny < 0 || ny >= columns) continue;
                                if (client_grid[nx][ny] == -1 || mine_marked[nx][ny]) {
                                    marked_around++;
                                } else if (client_grid[nx][ny] == -2 && cell_index[nx][ny] >= 0) {
                                    c.vars.push_back(cell_index[nx][ny]);
                                }
                            }
                            c.required = k - marked_around;
                            if (!c.vars.empty()) {
                                constraints.push_back(c);
                            }
                        }
                    }

                    // Backtrack to count solutions
                    int n = component.size();
                    total_solutions = 0;
                    memset(mine_count, 0, sizeof(mine_count));
                    backtrack(0, 0, n, constraints);

                    // Update probabilities
                    if (total_solutions > 0) {
                        for (int idx = 0; idx < n; idx++) {
                            int x = component[idx].first;
                            int y = component[idx].second;
                            double prob = (double)mine_count[idx] / total_solutions;
                            mine_prob[x][y] = prob;
                        }
                    }
                }
                // For larger components, leave them at base probability
            }
        }
    }

    // For any frontier cell that we didn't do full solving on, use simple probability from adjacent cells
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
                int x = cell.first;
                int y = cell.second;
                // If we haven't computed exact probability, or p is higher than current probability, update
                if (!in_frontier[x][y] || p > mine_prob[x][y]) {
                    mine_prob[x][y] = p;
                }
            }
        }
    }

    // Find the unknown cell with minimum probability of being mine
    // Tie-break: prefer cell with fewer unknown adjacent (more constrained)
    double min_prob = 1.0;
    int min_unknown_adj = 9;
    out_r = 0;
    out_c = 0;

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
            if (client_grid[i][j] != -2) continue;

            if (mine_prob[i][j] < min_prob) {
                int unknown_adj = 0;
                for (int d = 0; d < 8; d++) {
                    int ni = i + client_dx[d];
                    int nj = j + client_dy[d];
                    if (ni >= 0 && ni < rows && nj >= 0 && nj < columns && client_grid[ni][nj] == -2) {
                        unknown_adj++;
                    }
                }
                min_prob = mine_prob[i][j];
                min_unknown_adj = unknown_adj;
                out_r = i;
                out_c = j;
            } else if (mine_prob[i][j] == min_prob) {
                int unknown_adj = 0;
                for (int d = 0; d < 8; d++) {
                    int ni = i + client_dx[d];
                    int nj = j + client_dy[d];
                    if (ni >= 0 && ni < rows && nj >= 0 && nj < columns && client_grid[ni][nj] == -2) {
                        unknown_adj++;
                    }
                }
                if (unknown_adj < min_unknown_adj) {
                    min_unknown_adj = unknown_adj;
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

    // No obvious moves, use constraint-based probability guessing with connected component optimization
    find_best_guess(r, c, type);
    Execute(r, c, type);
}

#endif
