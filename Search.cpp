#include <thread>
#include <mutex>
#include <shared_mutex>

#include "Evaluate.cpp"
#include "Random.cpp"

using namespace std;
using arr = vector <vector <int>>;

typedef shared_mutex Lock;
typedef unique_lock <Lock> WriteLock;
typedef shared_lock <Lock> ReadLock;

Lock myLock;

arr solution (8, vector <int> (14));




int search() {
    ReadLock r_lock(myLock);

    int result, best = 0;

    while(true) {
        arr candidate = solution;

        const int x = random(8);
        const int y = random(14);
        const int val = candidate[x][y];

        candidate[x][y] = random(10);

        result = evaluate(candidate);

        candidate[x][y] = val;
    }

}
