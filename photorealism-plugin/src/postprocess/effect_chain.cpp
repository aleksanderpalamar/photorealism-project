#include "effect_chain.hpp"

namespace photorealism {
namespace {

ChainSlot next_intermediate(ChainSlot source) {
    return source == ChainSlot::First ? ChainSlot::Second : ChainSlot::First;
}

void append(EffectChain* chain, EffectPass pass, bool wanted) {
    if (!wanted) {
        return;
    }
    const ChainSlot source = chain->count == 0
                                 ? ChainSlot::Scene
                                 : chain->steps[chain->count - 1].target;
    chain->steps[chain->count] = {pass, source, next_intermediate(source)};
    ++chain->count;
}

}

EffectChain plan_effect_chain(const EffectFlags& flags) {
    EffectChain chain = {};
    append(&chain, EffectPass::Fxaa, flags.fxaa);
    append(&chain, EffectPass::Visual, true);
    append(&chain, EffectPass::InteriorLight, flags.interior_light);
    append(&chain, EffectPass::Occlusion, flags.occlusion);
    append(&chain, EffectPass::Temporal, flags.temporal);
    append(&chain, EffectPass::Sharpen, flags.sharpen);
    chain.steps[chain.count - 1].target = ChainSlot::Output;
    return chain;
}

const EffectStep* find_step(const EffectChain& chain, EffectPass pass) {
    for (unsigned index = 0; index < chain.count; ++index) {
        if (chain.steps[index].pass == pass) {
            return &chain.steps[index];
        }
    }
    return nullptr;
}

bool writes_output(const EffectChain& chain, EffectPass pass) {
    const EffectStep* step = find_step(chain, pass);
    return step != nullptr && step->target == ChainSlot::Output;
}

bool reads_scene(const EffectChain& chain, EffectPass pass) {
    const EffectStep* step = find_step(chain, pass);
    return step != nullptr && step->source == ChainSlot::Scene;
}

}
