#pragma once

namespace photorealism {

enum class EffectPass : unsigned {
    Fxaa,
    Visual,
    InteriorLight,
    Occlusion,
    Temporal,
    Sharpen,
};

constexpr unsigned kEffectPassCount = 6;

enum class ChainSlot {
    Scene,
    First,
    Second,
    Output,
};

struct EffectFlags {
    bool fxaa;
    bool interior_light;
    bool occlusion;
    bool temporal;
    bool sharpen;
};

struct EffectStep {
    EffectPass pass;
    ChainSlot source;
    ChainSlot target;
};

struct EffectChain {
    EffectStep steps[kEffectPassCount];
    unsigned count;
};

EffectChain plan_effect_chain(const EffectFlags& flags);
const EffectStep* find_step(const EffectChain& chain, EffectPass pass);
bool writes_output(const EffectChain& chain, EffectPass pass);
bool reads_scene(const EffectChain& chain, EffectPass pass);

}
