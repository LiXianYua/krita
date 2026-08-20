#include "PkColor.h"

int main(int argc, char **)
{
    PkColor color(1, 2, 3);
    color.setRed(argc > 1 ? -1 : 4);
    return color.red() == 4 ? 0 : 1;
}
