#include "PkTimer.h"
#include <chrono>
struct CompressorShape { PkTimer timer; int fires=0; void start(){ timer.start(std::chrono::milliseconds(10), [this]{++fires;}); } void stop(){timer.stop();} };
int main(){ CompressorShape shape; shape.start(); shape.stop(); }
