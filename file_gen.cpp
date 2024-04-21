#include <bits/stdc++.h>
#include <random>
#include <ctime>
using namespace std;
int main(int argc, char *argv[]) {
    int SIZE = atoi(argv[1]);
    srand(time(NULL));
    for (long long i = 0; i < SIZE; i++) {
        char c = rand() % 254 + 1;
        cout.write(&c, 1);
    }
}