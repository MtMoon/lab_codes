#include <iostream>
#include <cstdlib>

using namespace std;

int main() {

    int a[5] = {0,1,2,3,4};
    cout << &a[2] << endl;
    cout << a+2 << endl;
    int* b = a+2;
    cout << b << endl;
    cout << *b << endl;
    return 0;

}
