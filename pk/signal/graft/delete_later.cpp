#include "PkObject.h"
#include "PkThreadCallQueue.h"
struct Payload { explicit Payload(bool *p): destroyed(p){} ~Payload(){*destroyed=true;} bool *destroyed; };
// Retained KisDeleteLaterWrapper<T*> ownership shape: deferred wrapper teardown
// owns deletion of its pointer payload.
struct KisDeleteLaterWrapperShape : PkObject {
    explicit KisDeleteLaterWrapperShape(Payload *p): value(p){}
    ~KisDeleteLaterWrapperShape() override { delete value; }
    Payload *value;
};
int main(){
    PkThreadCallQueue::warmUpCurrentThread();
    bool destroyed=false;
    (new KisDeleteLaterWrapperShape(new Payload(&destroyed)))->deleteLater();
    if (destroyed) return 1;
    PkThreadCallQueue::processPendingCalls();
    return destroyed ? 0 : 2;
}
