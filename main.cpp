#include <iostream>
#include "Search.cpp"

using namespace std;

int main() {
    const int num = 16;
    
    cout << "number of logical processors: " << thread::hardware_concurrency() << endl;
    cout << "number of threads: " << num << endl;
    
    vector <thread> threads (num);
    
    for(auto & th : threads) {
        th = thread(repeat);
    }
    
    for(auto & th : threads) {
        th.join();
    }
    
    return 0;
}
