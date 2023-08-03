#include <thread>
#include <mutex>
#include <shared_mutex>

using namespace std;

typedef shared_mutex Lock;
typedef unique_lock <Lock> WriteLock;
typedef shared_lock <Lock> ReadLock;

Lock myLock;

int solution[8][14];
int data[20000];

void read() {
    ReadLock r_lock(myLock);
    //Do reader stuff
}

void write() {
    WriteLock w_lock(myLock);
    //Do writer stuff
}
