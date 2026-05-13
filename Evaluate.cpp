#include <vector>

using namespace std;

using Array = vector <vector <int>>;

const int dx[8] = {1, 1, 1, 0, 0, -1, -1, -1};
const int dy[8] = {1, 0, -1, 1, -1, 1, 0, -1};


int evaluate(Array & matrix) {
    vector <bool> data (20000, false);

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
                            if(result4 < 20000) {
                                data[result4] = true;
                            }
                        }
                    }
                }
            }
        }
    }

    int result = 1;
    while(result < 20000 && data[result]) {
        ++result;
    }

    return result - 1;
}
