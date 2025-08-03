#ifndef MMOTEEWORLDS_STEP_COMPONENT_H
#define MMOTEEWORLDS_STEP_COMPONENT_H

#include <base/types.h>

#include <optional>

class IStepComponent
{
public:
    virtual ~IStepComponent() = default;
    virtual void OnStart() { }
    virtual void OnActive() { }
    virtual void OnEnd() { }
    virtual bool IsFinished() const noexcept { return true; }
    virtual std::optional<StepId> GetNextStepId() const { return std::nullopt; }
};

#endif // MMOTEEWORLDS_STEP_COMPONENT_H
