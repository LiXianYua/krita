// Independent Qt/native characterization of the private-data COW boundary.
#include <iostream>
#ifdef PK_QT_ORACLE
#include <QSharedDataPointer>
struct Payload : QSharedData {
#else
#include "../PkSharedDataPointer.h"
struct Payload {
#endif
    static int copies;
    static int alive;
    int value = 11;
    Payload() { ++alive; }
    Payload(const Payload &other) : value(other.value) { ++copies; ++alive; }
    ~Payload() { --alive; }
};
int Payload::copies = 0;
int Payload::alive = 0;
#ifdef PK_QT_ORACLE
using Pointer = QSharedDataPointer<Payload>;
#else
using Pointer = PkSharedDataPointer<Payload>;
#endif
int main()
{
    {
        Pointer first(new Payload);
        Pointer second(first);
        const Pointer &readFirst = first, &readSecond = second;
        std::cout << "shared-read " << readFirst->value << ' ' << readSecond->value
                  << ' ' << Payload::copies << ' ' << Payload::alive << '\n';
        second->value = 23;
        std::cout << "write-detach " << readFirst->value << ' ' << readSecond->value
                  << ' ' << Payload::copies << ' ' << Payload::alive << '\n';
        Pointer third;
        third = first;
        const int value = third->value; // Mutable access detaches even for a read.
        std::cout << "mutable-read " << value << ' ' << Payload::copies << ' ' << Payload::alive << '\n';
        third = second;
        (*third).value = 39;
        const Pointer &readThird = third;
        std::cout << "assignment " << readFirst->value << ' ' << readSecond->value << ' '
                  << readThird->value << ' ' << Payload::copies << ' ' << Payload::alive << '\n';
        third = third;
        std::cout << "self " << readThird->value << ' ' << Payload::copies << ' ' << Payload::alive << '\n';
    }
    std::cout << "destroy " << Payload::alive << '\n';
}
