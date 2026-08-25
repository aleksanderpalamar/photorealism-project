#include "../src/fsr_color_scoring.hpp"

#include <cassert>

using photorealism::fsr::ColorEvidenceInput;
using photorealism::fsr::ColorEvidenceLabel;
using photorealism::fsr::AutomaticSceneCandidateInput;
using photorealism::fsr::AutomaticCandidateSignature;
using photorealism::fsr::automatic_candidate_is_better;
using photorealism::fsr::automatic_candidate_can_lock;
using photorealism::fsr::automatic_candidate_signature_matches;
using photorealism::fsr::classify_color_target;
using photorealism::fsr::score_automatic_scene_candidate;
using photorealism::fsr::evaluate_fsr_execution_policy;

int main() {
    const auto presentation = classify_color_target(ColorEvidenceInput{
        1920, 1080, 1920, 1080, 1, 60, 60, 10000, 1, 9999, 10000, true});
    assert(presentation.label == ColorEvidenceLabel::presentation);
    assert(presentation.confidence == 10);

    const auto scene = classify_color_target(ColorEvidenceInput{
        2400,
        1352,
        1920,
        1080,
        118,
        1000,
        900,
        10000,
        100,
        9000,
        10000,
        false});
    assert(scene.label == ColorEvidenceLabel::probable_scene);
    assert(scene.area_per_mille == 1565);

    const auto interface_target = classify_color_target(ColorEvidenceInput{
        1920,
        1080,
        1920,
        1080,
        2,
        20,
        20,
        10000,
        9500,
        9990,
        10000,
        false});
    assert(
        interface_target.label == ColorEvidenceLabel::probable_interface);

    const auto reflection = classify_color_target(ColorEvidenceInput{
        512,
        512,
        1920,
        1080,
        10,
        500,
        400,
        10000,
        200,
        8000,
        10000,
        false});
    assert(reflection.label == ColorEvidenceLabel::probable_reflection);

    const auto unclassified = classify_color_target(ColorEvidenceInput{
        256, 256, 1920, 1080, 80, 1, 0, 10000, 5000, 5000, 10000, false});
    assert(unclassified.label == ColorEvidenceLabel::unclassified);

    const auto ats_automatic = score_automatic_scene_candidate(
        AutomaticSceneCandidateInput{
            2400, 1352, 10, 1, 0x28, 0, 1, 1,
            1920, 1080, 2400, 1352,
            40, 40, 800, 1000, 99, 100, 20, 100, false});
    assert(!ats_automatic.eligible);

    const auto ets_asymmetric = score_automatic_scene_candidate(
        AutomaticSceneCandidateInput{
            1920, 1352, 10, 1, 0x28, 0, 1, 1,
            1920, 1080, 1920, 1352,
            35, 30, 750, 1000, 100, 101, 20, 101, false});
    assert(!ets_asymmetric.eligible);

    const auto ets_without_depth = score_automatic_scene_candidate(
        AutomaticSceneCandidateInput{
            1920, 1352, 10, 1, 0x28, 0, 1, 1,
            1920, 1080, 0, 0,
            35, 30, 750, 1000, 100, 101, 20, 101, false});
    assert(!ets_without_depth.eligible);
    assert(!automatic_candidate_can_lock(ets_without_depth));

    const auto new_depth_correlated_family = score_automatic_scene_candidate(
        AutomaticSceneCandidateInput{
            2048, 1152, 10, 1, 0x28, 0, 1, 1,
            1920, 1080, 2048, 1152,
            35, 30, 750, 1000, 100, 101, 20, 101, false});
    assert(!new_depth_correlated_family.eligible);
    assert(!automatic_candidate_can_lock(new_depth_correlated_family));

    const auto dynamic_composition = score_automatic_scene_candidate(
        AutomaticSceneCandidateInput{
            1600, 900, 28, 1, 0x28, 0, 1, 1,
            1920, 1080, 1600, 900,
            35, 30, 750, 1000, 100, 101, 40, 101, false});
    assert(dynamic_composition.eligible);
    assert(dynamic_composition.depth_correlated);
    assert(automatic_candidate_can_lock(dynamic_composition));

    const auto dynamic_without_depth = score_automatic_scene_candidate(
        AutomaticSceneCandidateInput{
            1280, 720, 26, 1, 0x28, 0, 1, 1,
            1920, 1080, 0, 0,
            35, 30, 750, 1000, 100, 101, 80, 101, false});
    assert(dynamic_without_depth.eligible);
    assert(!automatic_candidate_can_lock(dynamic_without_depth));

    const auto no_direct_composition = score_automatic_scene_candidate(
        AutomaticSceneCandidateInput{
            1600, 900, 28, 1, 0x28, 0, 1, 1,
            1920, 1080, 1600, 900,
            35, 30, 750, 1000, 100, 101, 0, 0, false});
    assert(!no_direct_composition.eligible);

    const auto r16_direct_composition = score_automatic_scene_candidate(
        AutomaticSceneCandidateInput{
            1600, 900, 10, 1, 0x28, 0, 1, 1,
            1920, 1080, 1600, 900,
            35, 30, 750, 1000, 100, 101, 50, 101, false});
    assert(r16_direct_composition.eligible);

    const auto reflection_rejected = score_automatic_scene_candidate(
        AutomaticSceneCandidateInput{
            512, 512, 10, 1, 0x28, 0, 1, 1,
            1920, 1080, 1920, 1352,
            500, 500, 900, 1000, 100, 101, 20, 101, false});
    assert(!reflection_rejected.eligible);

    const auto ui_rejected = score_automatic_scene_candidate(
        AutomaticSceneCandidateInput{
            1920, 1080, 28, 1, 0x28, 0, 1, 1,
            1920, 1080, 1920, 1352,
            100, 100, 999, 1000, 100, 101, 20, 101, false});
    assert(!ui_rejected.eligible);

    const auto native_r11_scene = score_automatic_scene_candidate(
        AutomaticSceneCandidateInput{
            1920, 1080, 26, 1, 0x28, 0, 1, 1,
            1920, 1080, 1920, 1080,
            7200, 7200, 999, 1000, 100, 101, 7180, 101, false});
    assert(native_r11_scene.eligible);
    assert(native_r11_scene.native_composition);
    assert(!native_r11_scene.known_family);
    assert(automatic_candidate_can_lock(native_r11_scene));

    const auto native_unorm_rejected = score_automatic_scene_candidate(
        AutomaticSceneCandidateInput{
            1920, 1080, 28, 1, 0x28, 0, 1, 1,
            1920, 1080, 1920, 1080,
            7200, 7200, 999, 1000, 100, 101, 7180, 101, false});
    assert(!native_unorm_rejected.eligible);

    const auto native_r11_weak_composition = score_automatic_scene_candidate(
        AutomaticSceneCandidateInput{
            2560, 1440, 26, 1, 0x28, 0, 1, 1,
            2560, 1440, 2560, 1440,
            100, 100, 999, 1000, 100, 101, 11, 101, false});
    assert(!native_r11_weak_composition.eligible);

    const auto observed_supersampled_r11 = score_automatic_scene_candidate(
        AutomaticSceneCandidateInput{
            1920, 1352, 26, 1, 0x28, 0, 1, 1,
            1920, 1080, 1920, 1352,
            1000, 950, 999, 1000, 100, 101, 900, 101, false});
    assert(observed_supersampled_r11.eligible);
    assert(automatic_candidate_can_lock(observed_supersampled_r11));

    const auto new_supersampled_depth_correlated =
        score_automatic_scene_candidate(AutomaticSceneCandidateInput{
            2560, 1440, 26, 1, 0x28, 0, 1, 1,
            1920, 1080, 2560, 1440,
            1000, 950, 999, 1000, 100, 101, 900, 101, false});
    assert(new_supersampled_depth_correlated.eligible);
    assert(new_supersampled_depth_correlated.depth_correlated);
    assert(automatic_candidate_can_lock(new_supersampled_depth_correlated));

    const auto new_supersampled_without_depth =
        score_automatic_scene_candidate(AutomaticSceneCandidateInput{
            2560, 1440, 26, 1, 0x28, 0, 1, 1,
            1920, 1080, 0, 0,
            1000, 950, 999, 1000, 100, 101, 900, 101, false});
    assert(!new_supersampled_without_depth.eligible);

    const auto cube_rejected = score_automatic_scene_candidate(
        AutomaticSceneCandidateInput{
            2400, 1352, 10, 1, 0x28, 0x4, 1, 1,
            1920, 1080, 2400, 1352,
            40, 40, 800, 1000, 100, 101, 20, 101, false});
    assert(!cube_rejected.eligible);

    assert(automatic_candidate_is_better(
        ats_automatic, 40, 800, ets_asymmetric, 35, 750));

    constexpr AutomaticCandidateSignature original_signature = {
        1920, 1352, 10, 1920, 1080};
    static_assert(automatic_candidate_signature_matches(
        original_signature, {1920, 1352, 10, 1920, 1080}));
    static_assert(!automatic_candidate_signature_matches(
        original_signature, {2400, 1352, 10, 1920, 1080}));
    static_assert(!automatic_candidate_signature_matches(
        original_signature, {1920, 1352, 10, 2560, 1440}));
    constexpr auto quality_policy =
        evaluate_fsr_execution_policy(1600, 900, 1920, 1080);
    static_assert(quality_policy.execute);
    constexpr auto native_policy =
        evaluate_fsr_execution_policy(1920, 1080, 1920, 1080);
    static_assert(!native_policy.execute);
    constexpr auto unsafe_aspect =
        evaluate_fsr_execution_policy(1600, 1000, 1920, 1080);
    static_assert(!unsafe_aspect.execute);
    return 0;
}
