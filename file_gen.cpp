#include <bits/stdc++.h>
#include <random>
#include <ctime>
using namespace std;
const long long SIZE = 1000000;
int main() {
    srand(time(NULL));
    for (long long i = 0; i < SIZE; i++) {
        char c = rand() % 254 + 1;
        cout.write(&c, 1);
    }
}