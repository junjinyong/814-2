#include <iostream>
#include <thread>
#include <mutex>
#include <shared_mutex>

#include "Evaluate.cpp"
#include "Random.cpp"

using namespace std;
using Array = vector <vector <int>>;

typedef shared_mutex Lock;
typedef unique_lock <Lock> WriteLock;
typedef shared_lock <Lock> ReadLock;

Lock myLock;
mutex upgradeLock;

Array solution (8, vector <int> (14));


int best = 0;
double temperature = 3.0;

void search() {
    int result;
    Array candidate;
    
    //upgradeLock.lock();
    while(true) {
        ReadLock r_lock(myLock);
        //upgradeLock.unlock();
        
        candidate = solution;
        const int x = random(8);
        const int y = random(14);
        candidate[x][y] = random(10);
        
        result = evaluate(candidate);
        //upgradeLock.lock();
        if(update(best, result, temperature)) {
            temperature = temperature * 0.999;
            break;
        }
    }
    
    WriteLock w_lock(myLock);
    //upgradeLock.unlock();
    solution = candidate;
    best = result;
    if(rand() % 1000 == 0) {
        cout << "New record(" << this_thread::get_id() << "): " << best << endl;
    }
    
    if(false) {
        for(int i = 0; i < 8; ++i) {
            for(int j = 0; j < 14; ++j) {
                printf("%3d", solution[i][j]);
            }
            printf("\n");
        }
        printf("\n");
    }
}

void repeat() {
    for(int i = 0; i < 10000; ++i) {
        search();
    }
}
