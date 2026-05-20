#include <iostream>
#include <chrono>
#include "Search.cpp"

using namespace std;

int main() {
    constexpr int time_budget_seconds = 3600;
    const unsigned int num_logical_processors = thread::hardware_concurrency();
    const unsigned int num_threads = 10; // num_logical_processors >> 1;
    
    cout << "number of logical processors: " << num_logical_processors << endl;
    cout << "number of threads: " << num_threads << endl;
    cout << "time budget (seconds): " << time_budget_seconds << endl;

    set_run_time_budget(chrono::seconds(time_budget_seconds));
    
    vector <thread> threads (num_threads);
    
    for(auto & th : threads) {
        th = thread(repeat);
    }
    
    for(auto & th : threads) {
        th.join();
    }

    const Fitness best_fitness = best_fitness_snapshot();
    cout << "final best score: " << best_fitness.prefix_score << endl;
    cout << "final frontier hits: " << best_fitness.frontier_hits
         << ", cover_9999: " << best_fitness.cover_9999
         << endl;
    print_board(best_solution_snapshot());
    
    return 0;
}
