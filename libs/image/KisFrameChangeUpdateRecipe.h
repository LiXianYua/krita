#ifndef KISFRAMECHANGEUPDATERECIPE_H
#define KISFRAMECHANGEUPDATERECIPE_H

#include <PkRect.h>
#include <kis_time_span.h>

struct KisFrameChangeUpdateRecipe
{
    KisTimeSpan affectedRange;
    PkRect affectedRect;
    PkRect totalDirtyRect;

    void notify(KisNode *node) const;
};

#endif // KISFRAMECHANGEUPDATERECIPE_H
