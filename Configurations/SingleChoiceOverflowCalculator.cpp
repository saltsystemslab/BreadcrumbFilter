#include <iostream>
#include <algorithm>
#include <vector>
#include <cmath>
#include <random>

using namespace std;

using ll = long long;

ll calculateOverflow(ll N, ll B) {
    vector<ll> v(N / B);
    random_device rd;
    mt19937 generator (rd());
    uniform_int_distribution<size_t> bucket_dist(0, (N/B)-1);
    for (ll i = 0; i < N; i++) {
        v[bucket_dist(generator)]++;
    }
    return *std::max_element(v.begin(), v.end());
}

double singleBucketLoadFactor(ll N, ll B, ll NT = 10) {
    double sum = 0.0;
    for (ll i=0; i < NT; i++) {
        vector<ll> v(N / B);
        random_device rd;
        mt19937 generator (rd());
        uniform_int_distribution<size_t> bucket_dist(0, (N/B)-1);
        bool finished = false;
        for (ll i = 0; i < N; i++) {
            ll bucket_load = ++v[bucket_dist(generator)];
            if (bucket_load > B) {
                // return ((double(i)) / N);
                sum += (double(i)) / N;
                finished = true;
                break;
            }
        }
        if (!finished) {
            sum += 1;
        }
    }
    return sum / NT;
}

int main() {
    std::cout << calculateOverflow(1ll << 30, 64) << " " << singleBucketLoadFactor(1ll << 30, 64) << std::endl;
}