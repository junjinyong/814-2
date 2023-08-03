#include <iostream>
#include <thread>
#include <mutex>
#include <shared_mutex>

using namespace std;

typedef shared_mutex Lock;
typedef unique_lock <Lock> WriteLock;
typedef shared_lock <Lock> ReadLock;

Lock myLock;

void ReadFunction() {
    ReadLock r_lock(myLock);
    //Do reader stuff
}

void WriteFunction() {
    WriteLock w_lock(myLock);
    //Do writer stuff
}

int main() {
    cout << "number of logical processors: " << std::thread::hardware_concurrency() << endl;
    cout << "thread id: " << this_thread::get_id() << endl;
    
    
    
    return 0;
}
