#include "tool_transform.h"

#include <mutex>

void registerToolTransformPlugin(const TransformToolRegistrationCallback &callback)
{
    static std::once_flag once;
    std::call_once(once, [&callback] {
        callback(TransformToolRegistrationStep::ToolFactory);
        callback(TransformToolRegistrationStep::AnimatedParamsHolderFactory);
        callback(TransformToolRegistrationStep::TransformMaskFactory);
        callback(TransformToolRegistrationStep::DumbTransformMaskFactory);
    });
}
