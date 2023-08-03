#include <iostream>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include "Search.cpp"

using namespace std;



int main() {
    cout << "number of logical processors: " << thread::hardware_concurrency() << endl;
    cout << "thread id: " << this_thread::get_id() << endl;
    
    
    
    return 0;
}
