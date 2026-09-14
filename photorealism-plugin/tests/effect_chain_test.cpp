#include "../src/postprocess/effect_chain.hpp"

#include <cassert>
#include <cstdio>

using namespace photorealism;

namespace {

void only_the_grade_writes_straight_to_the_screen() {
    const EffectChain chain = plan_effect_chain({false, false, false, false, false});
    assert(chain.count == 1);
    assert(chain.steps[0].pass == EffectPass::Visual);
    assert(chain.steps[0].source == ChainSlot::Scene);
    assert(chain.steps[0].target == ChainSlot::Output);
    assert(reads_scene(chain, EffectPass::Visual));
    assert(writes_output(chain, EffectPass::Visual));
}

void every_pass_reads_what_the_previous_wrote() {
    const EffectChain chain = plan_effect_chain({true, true, true, true, true});
    const EffectPass order[] = {
        EffectPass::Fxaa, EffectPass::Visual, EffectPass::InteriorLight,
        EffectPass::Occlusion, EffectPass::Temporal, EffectPass::Sharpen};
    assert(chain.count == 6);
    for (unsigned index = 0; index < chain.count; ++index) {
        assert(chain.steps[index].pass == order[index]);
        assert(chain.steps[index].source != chain.steps[index].target);
        if (index > 0) {
            assert(chain.steps[index].source == chain.steps[index - 1].target);
        }
    }
    assert(chain.steps[0].source == ChainSlot::Scene);
    assert(chain.steps[5].target == ChainSlot::Output);
    assert(!reads_scene(chain, EffectPass::Visual));
    assert(!writes_output(chain, EffectPass::Temporal));
}

void the_temporal_history_never_holds_the_sharpening() {
    const EffectChain sharp = plan_effect_chain({false, false, true, true, true});
    assert(find_step(sharp, EffectPass::Temporal)->target != ChainSlot::Output);
    assert(writes_output(sharp, EffectPass::Sharpen));
    const EffectChain plain = plan_effect_chain({false, false, true, true, false});
    assert(writes_output(plain, EffectPass::Temporal));
    assert(find_step(plain, EffectPass::Sharpen) == nullptr);
}

}

int main() {
    only_the_grade_writes_straight_to_the_screen();
    every_pass_reads_what_the_previous_wrote();
    the_temporal_history_never_holds_the_sharpening();
    std::printf("effect_chain_test ok\n");
    return 0;
}
