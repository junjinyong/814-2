#include <algorithm>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "Evaluate.cpp"
#include "Random.cpp"

using namespace std;
using Array = vector <vector <int>>;

namespace {

constexpr int rows = 8;
constexpr int cols = 14;
constexpr int digits = 10;
constexpr int iterations_per_thread = 30000;
constexpr int reheat_after = 2000;
constexpr double initial_temperature = 0.20;
constexpr double cooling_rate = 0.9995;
constexpr double minimum_temperature = 0.002;
constexpr int elite_archive_capacity = 16;
constexpr int elite_restart_choices = 12;
constexpr int elite_restart_mutations = 6;
constexpr int random_restart_percent = 20;
constexpr int local_improvement_rounds = 1;
constexpr int local_improvement_probes = 2;

struct EliteEntry {
    Array board;
    Fitness fitness;
};

mutex state_lock;
Array global_best_solution(rows, vector <int> (cols, 0));
Fitness global_best_fitness;
vector <EliteEntry> elite_archive;

Array random_board() {
    Array board(rows, vector <int> (cols));

    for(int i = 0; i < rows; ++i) {
        for(int j = 0; j < cols; ++j) {
            board[i][j] = random(digits);
        }
    }

    return board;
}

void mutate_one_cell(Array & board) {
    const int x = random(rows);
    const int y = random(cols);
    const int old_digit = board[x][y];

    int new_digit = old_digit;
    while(new_digit == old_digit) {
        new_digit = random(digits);
    }
    board[x][y] = new_digit;
}

void mutate_swap_cells(Array & board) {
    for(int attempt = 0; attempt < 8; ++attempt) {
        const int x1 = random(rows);
        const int y1 = random(cols);
        const int x2 = random(rows);
        const int y2 = random(cols);

        if((x1 != x2 || y1 != y2) && board[x1][y1] != board[x2][y2]) {
            swap(board[x1][y1], board[x2][y2]);
            return;
        }
    }

    mutate_one_cell(board);
}

void mutate_patch(Array & board) {
    const int height = 1 + random(2);
    const int width = 2 + random(2);
    const int start_x = random(rows - height + 1);
    const int start_y = random(cols - width + 1);

    bool changed = false;
    for(int dx = 0; dx < height; ++dx) {
        for(int dy = 0; dy < width; ++dy) {
            const int x = start_x + dx;
            const int y = start_y + dy;
            const int old_digit = board[x][y];
            int new_digit = old_digit;

            while(new_digit == old_digit) {
                new_digit = random(digits);
            }
            board[x][y] = new_digit;
            changed = true;
        }
    }

    if(!changed) {
        mutate_one_cell(board);
    }
}

void apply_search_move(Array & board, int stagnant_steps) {
    const int roll = random(100);
    if(stagnant_steps < reheat_after / 2) {
        if(roll < 75) {
            mutate_one_cell(board);
        } else if(roll < 95) {
            mutate_swap_cells(board);
        } else {
            mutate_patch(board);
        }
    } else {
        if(roll < 45) {
            mutate_one_cell(board);
        } else if(roll < 75) {
            mutate_swap_cells(board);
        } else {
            mutate_patch(board);
        }
    }
}

void apply_refinement_move(Array & board) {
    if(random(100) < 70) {
        mutate_one_cell(board);
    } else {
        mutate_swap_cells(board);
    }
}

void local_improve(Array & board, Fitness & fitness) {
    for(int round = 0; round < local_improvement_rounds; ++round) {
        Array best_trial = board;
        Fitness best_trial_fitness = fitness;
        bool improved = false;

        for(int probe = 0; probe < local_improvement_probes; ++probe) {
            Array trial = board;
            apply_refinement_move(trial);
            const Fitness trial_fitness = evaluate(trial);
            if(trial_fitness > best_trial_fitness) {
                best_trial = move(trial);
                best_trial_fitness = trial_fitness;
                improved = true;
            }
        }

        if(!improved) {
            break;
        }

        board = move(best_trial);
        fitness = best_trial_fitness;
    }
}

void insert_into_elite_archive(const Array & board, const Fitness & fitness) {
    lock_guard <mutex> lock(state_lock);

    elite_archive.push_back({board, fitness});
    stable_sort(elite_archive.begin(), elite_archive.end(), [] (const EliteEntry & lhs, const EliteEntry & rhs) {
        return lhs.fitness > rhs.fitness;
    });

    if(static_cast <int> (elite_archive.size()) > elite_archive_capacity) {
        elite_archive.resize(elite_archive_capacity);
    }
}

Array elite_restart_board() {
    lock_guard <mutex> lock(state_lock);
    if(elite_archive.empty()) {
        return random_board();
    }

    const int choices = min(static_cast <int> (elite_archive.size()), elite_restart_choices);
    return elite_archive[random(choices)].board;
}

Array restart_seed_board() {
    if(random(100) < random_restart_percent) {
        return random_board();
    }

    return elite_restart_board();
}

void publish_global_best(const Array & board, const Fitness & fitness) {
    lock_guard <mutex> lock(state_lock);
    if(fitness <= global_best_fitness) {
        return;
    }

    global_best_solution = board;
    global_best_fitness = fitness;
    cout << "New global best(" << this_thread::get_id() << "): "
         << "prefix=" << fitness.prefix_score
         << ", frontier=" << fitness.frontier_hits
         << ", cover_9999=" << fitness.cover_9999
         << endl;
}

}  // namespace

void print_board(const Array & board) {
    for(int i = 0; i < rows; ++i) {
        for(int j = 0; j < cols; ++j) {
            cout << board[i][j];
        }
        cout << '\n';
    }
}

Fitness best_fitness_snapshot() {
    lock_guard <mutex> lock(state_lock);
    return global_best_fitness;
}

Array best_solution_snapshot() {
    lock_guard <mutex> lock(state_lock);
    return global_best_solution;
}

void search() {
    Array current = random_board();
    Fitness current_fitness = evaluate(current);

    Array local_best = current;
    Fitness local_best_fitness = current_fitness;

    double temperature = initial_temperature;
    int stagnant_steps = 0;

    insert_into_elite_archive(local_best, local_best_fitness);
    publish_global_best(local_best, local_best_fitness);

    for(int iter = 0; iter < iterations_per_thread; ++iter) {
        Array candidate = current;
        apply_search_move(candidate, stagnant_steps);

        Fitness candidate_fitness = evaluate(candidate);
        const Fitness previous_fitness = current_fitness;
        if(update(annealing_value(previous_fitness), annealing_value(candidate_fitness), temperature)) {
            current = move(candidate);
            current_fitness = candidate_fitness;

            if(current_fitness >= previous_fitness
                && (current_fitness.prefix_score > previous_fitness.prefix_score
                    || stagnant_steps >= reheat_after / 2)) {
                local_improve(current, current_fitness);
            }
        }

        if(current_fitness > local_best_fitness) {
            local_best = current;
            local_best_fitness = current_fitness;
            stagnant_steps = 0;
            insert_into_elite_archive(local_best, local_best_fitness);
            publish_global_best(local_best, local_best_fitness);
        } else {
            ++stagnant_steps;
        }

        temperature = max(minimum_temperature, temperature * cooling_rate);
        if(stagnant_steps >= reheat_after) {
            current = restart_seed_board();
            for(int mutation = 0; mutation < elite_restart_mutations; ++mutation) {
                apply_search_move(current, reheat_after);
            }
            current_fitness = evaluate(current);
            local_improve(current, current_fitness);
            if(current_fitness > local_best_fitness) {
                local_best = current;
                local_best_fitness = current_fitness;
                insert_into_elite_archive(local_best, local_best_fitness);
                publish_global_best(local_best, local_best_fitness);
            }
            temperature = initial_temperature;
            stagnant_steps = 0;
        }
    }
}

void repeat() {
    search();
}
