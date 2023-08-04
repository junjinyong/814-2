#include <random>

using namespace std;

random_device rd;
mt19937 gen(rd());
uniform_int_distribution <int> dis(1, 8140);
mt19937_64 seed(rd());
uniform_real_distribution <double> rng(0, 1);

inline int random(int mod) {
    return dis(gen) % mod;
}

inline bool update(int best, int result, double temperature) {
    return exp((result - best) / (temperature + 0.02)) > rng(seed);
}
