#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace photorealism {
namespace shader_patch {

enum class OperandType : std::uint32_t {
    temp = 0,
    input = 1,
    output = 2,
    indexable_temp = 3,
    immediate32 = 4,
    immediate64 = 5,
    sampler = 6,
    resource = 7,
    constant_buffer = 8,
    immediate_constant_buffer = 9,
    label = 10,
    input_primitive_id = 11,
    output_depth = 12,
    null = 13,
};

enum class SelectionMode : std::uint8_t {
    none = 0,
    mask = 1,
    swizzle = 2,
    select_one = 3,
};

enum class Modifier : std::uint8_t {
    none = 0,
    negate = 1,
    absolute = 2,
    absolute_negate = 3,
};

struct Index {
    std::uint32_t immediate = 0;
    bool relative = false;
    std::uint32_t relative_register = 0;
    std::uint8_t relative_component = 0;
};

struct Operand {
    OperandType type = OperandType::temp;
    SelectionMode selection = SelectionMode::none;
    Modifier modifier = Modifier::none;
    std::uint8_t mask = 0;
    std::uint8_t swizzle[4] = {0, 1, 2, 3};
    std::uint8_t select_one = 0;
    std::uint8_t component_count = 0;
    std::vector<Index> indices;
    float immediate_float[4] = {0, 0, 0, 0};
    std::uint32_t immediate_raw[4] = {0, 0, 0, 0};
};

struct Instruction {
    std::uint32_t opcode = 0;
    bool saturate = false;
    std::uint32_t control = 0;
    std::vector<Operand> operands;
};

namespace opcode {
constexpr std::uint32_t kAdd = 0;
constexpr std::uint32_t kAnd = 1;
constexpr std::uint32_t kBreak = 2;
constexpr std::uint32_t kBreakc = 3;
constexpr std::uint32_t kCase = 6;
constexpr std::uint32_t kContinue = 7;
constexpr std::uint32_t kContinuec = 8;
constexpr std::uint32_t kDefault = 10;
constexpr std::uint32_t kDerivRtx = 11;
constexpr std::uint32_t kDerivRty = 12;
constexpr std::uint32_t kDiscard = 13;
constexpr std::uint32_t kDiv = 14;
constexpr std::uint32_t kDp2 = 15;
constexpr std::uint32_t kDp3 = 16;
constexpr std::uint32_t kDp4 = 17;
constexpr std::uint32_t kElse = 18;
constexpr std::uint32_t kEndif = 21;
constexpr std::uint32_t kEndloop = 22;
constexpr std::uint32_t kEndswitch = 23;
constexpr std::uint32_t kEq = 24;
constexpr std::uint32_t kExp = 25;
constexpr std::uint32_t kIf = 31;
constexpr std::uint32_t kFrc = 26;
constexpr std::uint32_t kFtoi = 27;
constexpr std::uint32_t kFtou = 28;
constexpr std::uint32_t kGe = 29;
constexpr std::uint32_t kIadd = 30;
constexpr std::uint32_t kIeq = 32;
constexpr std::uint32_t kIge = 33;
constexpr std::uint32_t kIlt = 34;
constexpr std::uint32_t kImad = 35;
constexpr std::uint32_t kImax = 36;
constexpr std::uint32_t kImin = 37;
constexpr std::uint32_t kImul = 38;
constexpr std::uint32_t kIne = 39;
constexpr std::uint32_t kIneg = 40;
constexpr std::uint32_t kIshl = 41;
constexpr std::uint32_t kIshr = 42;
constexpr std::uint32_t kItof = 43;
constexpr std::uint32_t kLd = 45;
constexpr std::uint32_t kLog = 47;
constexpr std::uint32_t kLoop = 48;
constexpr std::uint32_t kLt = 49;
constexpr std::uint32_t kMad = 50;
constexpr std::uint32_t kMin = 51;
constexpr std::uint32_t kMax = 52;
constexpr std::uint32_t kCustomData = 53;
constexpr std::uint32_t kMov = 54;
constexpr std::uint32_t kMovc = 55;
constexpr std::uint32_t kMul = 56;
constexpr std::uint32_t kNe = 57;
constexpr std::uint32_t kNop = 58;
constexpr std::uint32_t kNot = 59;
constexpr std::uint32_t kOr = 60;
constexpr std::uint32_t kResInfo = 61;
constexpr std::uint32_t kRet = 62;
constexpr std::uint32_t kRoundNe = 64;
constexpr std::uint32_t kRoundNi = 65;
constexpr std::uint32_t kRoundPi = 66;
constexpr std::uint32_t kRoundZ = 67;
constexpr std::uint32_t kRsq = 68;
constexpr std::uint32_t kSample = 69;
constexpr std::uint32_t kSampleC = 70;
constexpr std::uint32_t kSampleCLz = 71;
constexpr std::uint32_t kSampleL = 72;
constexpr std::uint32_t kSampleD = 73;
constexpr std::uint32_t kSampleB = 74;
constexpr std::uint32_t kSqrt = 75;
constexpr std::uint32_t kSwitch = 76;
constexpr std::uint32_t kSinCos = 77;
constexpr std::uint32_t kRetc = 63;
constexpr std::uint32_t kUdiv = 78;
constexpr std::uint32_t kUlt = 79;
constexpr std::uint32_t kUge = 80;
constexpr std::uint32_t kUmul = 81;
constexpr std::uint32_t kUmad = 82;
constexpr std::uint32_t kUmax = 83;
constexpr std::uint32_t kUmin = 84;
constexpr std::uint32_t kUshr = 85;
constexpr std::uint32_t kUtof = 86;
constexpr std::uint32_t kXor = 87;
constexpr std::uint32_t kDclResource = 88;
constexpr std::uint32_t kDclConstantBuffer = 89;
constexpr std::uint32_t kDclSampler = 90;
constexpr std::uint32_t kDclInputPs = 98;
constexpr std::uint32_t kDclInputPsSgv = 99;
constexpr std::uint32_t kDclInputPsSiv = 100;
constexpr std::uint32_t kDclOutput = 101;
constexpr std::uint32_t kDclTemps = 104;
constexpr std::uint32_t kDclIndexableTemp = 105;
constexpr std::uint32_t kDclGlobalFlags = 106;
constexpr std::uint32_t kDerivRtxCoarse = 122;
constexpr std::uint32_t kDerivRtxFine = 123;
constexpr std::uint32_t kDerivRtyCoarse = 124;
constexpr std::uint32_t kDerivRtyFine = 125;
constexpr std::uint32_t kRcp = 129;
constexpr std::uint32_t kF32ToF16 = 130;
constexpr std::uint32_t kF16ToF32 = 131;
constexpr std::uint32_t kCountBits = 134;
constexpr std::uint32_t kUbfe = 138;
constexpr std::uint32_t kIbfe = 139;
constexpr std::uint32_t kBfi = 140;
constexpr std::uint32_t kBfRev = 141;
}

struct Program {
    std::uint32_t major = 0;
    std::uint32_t minor = 0;
    std::uint32_t program_type = 0;
    std::uint32_t temp_count = 0;
    std::vector<Instruction> instructions;
    std::vector<std::uint32_t> declared_samplers;
    std::vector<std::uint32_t> declared_resources;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> declared_buffers;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> indexable_temps;
};

bool decode_program(const std::uint8_t* code, std::size_t size, Program* out);

}
}
