#include <vector>

using namespace std;

using Array = vector <vector <int>>;

constexpr int evaluation_limit = 20000;
constexpr int coverage_limit = 9999;
constexpr int frontier_window = 256;

struct Fitness {
    int prefix_score = -1;
    int frontier_hits = 0;
    int cover_9999 = 0;
};

const int dx[8] = {1, 1, 1, 0, 0, -1, -1, -1};
const int dy[8] = {1, 0, -1, 1, -1, 1, 0, -1};

inline bool operator<(const Fitness & lhs, const Fitness & rhs) {
    if(lhs.prefix_score != rhs.prefix_score) {
        return lhs.prefix_score < rhs.prefix_score;
    }
    if(lhs.frontier_hits != rhs.frontier_hits) {
        return lhs.frontier_hits < rhs.frontier_hits;
    }
    return lhs.cover_9999 < rhs.cover_9999;
}

inline bool operator>(const Fitness & lhs, const Fitness & rhs) {
    return rhs < lhs;
}

inline bool operator<=(const Fitness & lhs, const Fitness & rhs) {
    return !(rhs < lhs);
}

inline bool operator>=(const Fitness & lhs, const Fitness & rhs) {
    return !(lhs < rhs);
}

inline bool operator==(const Fitness & lhs, const Fitness & rhs) {
    return lhs.prefix_score == rhs.prefix_score
        && lhs.frontier_hits == rhs.frontier_hits
        && lhs.cover_9999 == rhs.cover_9999;
}

inline double annealing_value(const Fitness & fitness) {
    constexpr double frontier_scale = 1.0 / static_cast <double> (frontier_window + 1);
    constexpr double coverage_scale = frontier_scale / static_cast <double> (coverage_limit + 1);

    return static_cast <double> (fitness.prefix_score)
        + static_cast <double> (fitness.frontier_hits) * frontier_scale
        + static_cast <double> (fitness.cover_9999) * coverage_scale;
}

Fitness evaluate(const Array & matrix) {
    vector <bool> data (evaluation_limit, false);

    for(int i = 0; i < 8; ++i) {
        for(int j = 0; j < 14; ++j) {
            const int result0 = matrix[i][j];
            data[result0] = true;

            for(int k = 0; k < 8; ++k) {
                const int x1 = i + dx[k];
                const int y1 = j + dy[k];
                if(x1 < 0 || x1 >= 8 || y1 < 0 || y1 >= 14) {
                    continue;
                }

                const int result1 = 10 * result0 + matrix[x1][y1];
                data[result1] = true;

                for(int l = 0; l < 8; ++l) {
                    const int x2 = x1 + dx[l];
                    const int y2 = y1 + dy[l];
                    if(x2 < 0 || x2 >= 8 || y2 < 0 || y2 >= 14) {
                        continue;
                    }

                    const int result2 = 10 * result1 + matrix[x2][y2];
                    data[result2] = true;

                    for(int m = 0; m < 8; ++m) {
                        const int x3 = x2 + dx[m];
                        const int y3 = y2 + dy[m];
                        if(x3 < 0 || x3 >= 8 || y3 < 0 || y3 >= 14) {
                            continue;
                        }

                        const int result3 = 10 * result2 + matrix[x3][y3];
                        data[result3] = true;

                        for(int n = 0; n < 8; ++n) {
                            const int x4 = x3 + dx[n];
                            const int y4 = y3 + dy[n];
                            if(x4 < 0 || x4 >= 8 || y4 < 0 || y4 >= 14) {
                                continue;
                            }

                            const int result4 = 10 * result3 + matrix[x4][y4];
                            if(result4 < evaluation_limit) {
                                data[result4] = true;
                            }
                        }
                    }
                }
            }
        }
    }

    Fitness fitness;

    int first_missing = 1;
    while(first_missing < evaluation_limit && data[first_missing]) {
        ++first_missing;
    }
    fitness.prefix_score = first_missing - 1;

    for(int value = 1; value <= coverage_limit; ++value) {
        if(data[value]) {
            ++fitness.cover_9999;
        }
    }

    const int frontier_end = min(evaluation_limit - 1, fitness.prefix_score + frontier_window);
    for(int value = fitness.prefix_score + 1; value <= frontier_end; ++value) {
        if(data[value]) {
            ++fitness.frontier_hits;
        }
    }

    return fitness;
}
