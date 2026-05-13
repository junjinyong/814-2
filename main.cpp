#include <iostream>
#include "Search.cpp"

using namespace std;

int main() {
    const unsigned int num_logical_processors = thread::hardware_concurrency();
    const unsigned int num_threads = 10; // num_logical_processors >> 1;
    
    cout << "number of logical processors: " << num_logical_processors << endl;
    cout << "number of threads: " << num_threads << endl;
    
    vector <thread> threads (num_threads);
    
    for(auto & th : threads) {
        th = thread(repeat);
    }
    
    for(auto & th : threads) {
        th.join();
    }

    cout << "final best score: " << best_score_snapshot() << endl;
    print_board(best_solution_snapshot());
    
    return 0;
}
