#include <random>

using namespace std;

random_device rd;
mt19937 gen(rd());
uniform_int_distribution <int> dis(1, 8140);




inline int random(int mod) {
    return dis(gen) % mod;
}
