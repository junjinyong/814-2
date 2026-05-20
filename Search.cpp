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
constexpr int elite_restart_choices = 4;
constexpr int elite_restart_mutations = 4;

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
        mutate_one_cell(candidate);

        const Fitness candidate_fitness = evaluate(candidate);
        if(update(annealing_value(current_fitness), annealing_value(candidate_fitness), temperature)) {
            current = move(candidate);
            current_fitness = candidate_fitness;
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
            current = elite_restart_board();
            for(int mutation = 0; mutation < elite_restart_mutations; ++mutation) {
                mutate_one_cell(current);
            }
            current_fitness = evaluate(current);
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
