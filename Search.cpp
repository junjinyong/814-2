#include <algorithm>
#include <iostream>
#include <mutex>
#include <thread>

#include "Evaluate.cpp"
#include "Random.cpp"

using namespace std;
using Array = vector <vector <int>>;

namespace {

constexpr int rows = 8;
constexpr int cols = 14;
constexpr int digits = 10;
constexpr int iterations_per_thread = 10000;
constexpr int reheat_after = 2000;
constexpr double initial_temperature = 3.0;
constexpr double cooling_rate = 0.9995;
constexpr double minimum_temperature = 0.05;

mutex best_lock;
Array global_best_solution(rows, vector <int> (cols, 0));
int global_best_score = -1;

Array random_board() {
    Array board(rows, vector <int> (cols));

    for(int i = 0; i < rows; ++i) {
        for(int j = 0; j < cols; ++j) {
            board[i][j] = random(digits);
        }
    }

    return board;
}

void publish_global_best(const Array & board, int score) {
    lock_guard <mutex> lock(best_lock);
    if(score <= global_best_score) {
        return;
    }

    global_best_solution = board;
    global_best_score = score;
    cout << "New global best(" << this_thread::get_id() << "): " << score << endl;
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

int best_score_snapshot() {
    lock_guard <mutex> lock(best_lock);
    return global_best_score;
}

Array best_solution_snapshot() {
    lock_guard <mutex> lock(best_lock);
    return global_best_solution;
}

void search() {
    Array current = random_board();
    int current_score = evaluate(current);

    Array local_best = current;
    int local_best_score = current_score;

    double temperature = initial_temperature;
    int stagnant_steps = 0;

    publish_global_best(local_best, local_best_score);

    for(int iter = 0; iter < iterations_per_thread; ++iter) {
        Array candidate = current;

        const int x = random(rows);
        const int y = random(cols);
        const int old_digit = candidate[x][y];

        int new_digit = old_digit;
        while(new_digit == old_digit) {
            new_digit = random(digits);
        }
        candidate[x][y] = new_digit;

        const int candidate_score = evaluate(candidate);
        if(update(current_score, candidate_score, temperature)) {
            current = move(candidate);
            current_score = candidate_score;
        }

        if(current_score > local_best_score) {
            local_best = current;
            local_best_score = current_score;
            stagnant_steps = 0;
            publish_global_best(local_best, local_best_score);
        } else {
            ++stagnant_steps;
        }

        temperature = max(minimum_temperature, temperature * cooling_rate);
        if(stagnant_steps >= reheat_after) {
            current = local_best;
            current_score = local_best_score;
            temperature = initial_temperature;
            stagnant_steps = 0;
        }
    }
}

void repeat() {
    search();
}
