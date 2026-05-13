#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <random>
#include <thread>

using namespace std;

inline mt19937 & generator() {
    thread_local mt19937 gen([] {
        random_device rd;
        const auto now = static_cast <unsigned int> (chrono::steady_clock::now().time_since_epoch().count());
        const auto tid = static_cast <unsigned int> (hash <thread::id> {}(this_thread::get_id()));
        seed_seq seed {rd(), rd(), rd(), rd(), now, tid};
        return mt19937(seed);
    }());

    return gen;
}

inline int random(int mod) {
    uniform_int_distribution <int> dist(0, mod - 1);
    return dist(generator());
}

inline double random_unit() {
    uniform_real_distribution <double> dist(0.0, 1.0);
    return dist(generator());
}

inline bool update(int current, int candidate, double temperature) {
    if(candidate >= current) {
        return true;
    }

    const double safe_temperature = max(temperature, 1e-6);
    return exp(static_cast <double> (candidate - current) / safe_temperature) > random_unit();
}
