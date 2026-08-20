#include "PkObject.h"
struct Wrapper : PkObject {};
int main(){ (new Wrapper)->deleteLater(); }
