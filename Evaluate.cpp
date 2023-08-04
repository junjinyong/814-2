#include <vector>

using namespace std;

using Array = vector <vector <int>>;

const int dx[4] = {1, 0, -1, 0};
const int dy[4] = {0, 1, 0, -1};


int evaluate(Array & matrix) {
    vector <bool> data (20000, false);
    int result;

    for(int i = 0; i < 8; ++i) {
        for(int j = 0; j < 14; ++j) {
            result = matrix[i][j];
            data[result] = true;

            for(int k = 0; k < 4; ++k) {
                const int x1 = i + dx[k];
                const int y1 = j + dy[k];
                if(x1 < 0 || x1 >= 8 || y1 < 0 || y1 >= 14) {
                    continue;
                }

                result = 10 * result + matrix[x1][y1];
                data[result] = true;

                for(int l = 0; l < 4; ++l) {
                    const int x2 = x1 + dx[l];
                    const int y2 = y1 + dy[l];
                    if(x2 < 0 || x2 >= 8 || y2 < 0 || y2 >= 14) {
                        continue;
                    }

                    result = 10 * result + matrix[x2][y2];
                    data[result] = true;


                    for(int m = 0; m < 4; ++m) {
                        const int x3 = x2 + dx[l];
                        const int y3 = y2 + dy[l];
                        if(x3 < 0 || x3 >= 8 || y3 < 0 || y3 >= 14) {
                            continue;
                        }

                        result = 10 * result + matrix[x3][y3];
                        data[result] = true;

                        for(int n = 0; n < 4; ++n) {
                            const int x4 = x3 + dx[l];
                            const int y4 = y3 + dy[l];
                            if(x4 < 0 || x4 >= 8 || y4 < 0 || y4 >= 14) {
                                continue;
                            }

                            const int temp = 10 * result + matrix[x3][y3];
                            if(temp < 20000) {
                                data[temp] = true;
                            }
                        }
                        result = result / 10;
                    }
                    result = result / 10;
                }
                result = result / 10;
            }
        }
    }

    for(result = 0; result < 20000 && data[result]; ++result);

    return result;
}
