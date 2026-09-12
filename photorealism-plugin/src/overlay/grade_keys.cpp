#include "grade_keys.hpp"

namespace photorealism {
namespace overlay {

const char* grade_key_for(float Settings::*member) {
    for (std::size_t index = 0; index < kGradeKeyCount; ++index) {
        if (kGradeKeys[index].member == member) {
            return kGradeKeys[index].key;
        }
    }
    return nullptr;
}

}
}
