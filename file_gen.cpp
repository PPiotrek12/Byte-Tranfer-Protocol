#include <bits/stdc++.h>
using namespace std;
const int SIZE = 2000;
char buffer[SIZE];
int main() {
    int n = 250;
    char a = 'a';
    for (int i = 0; i < 249; i++) {
        memcpy(buffer + 4 * i, &i, 4);
    }
    cout.write(buffer, SIZE);
}