#pragma once

#include <cstddef>
#include <string>

namespace photorealism::aa_config {

inline bool quoted_value_range(
    const std::string& contents,
    const char* key,
    std::size_t* value_begin,
    std::size_t* value_end) {
    const std::string marker = std::string("uset ") + key;
    std::size_t line = 0;
    while (line < contents.size()) {
        std::size_t first = line;
        while (first < contents.size() &&
               (contents[first] == ' ' || contents[first] == '\t')) {
            ++first;
        }
        const std::size_t line_end = contents.find('\n', first);
        const std::size_t end =
            line_end == std::string::npos ? contents.size() : line_end;
        const std::size_t after_marker = first + marker.size();
        if (end - first > marker.size() &&
            contents.compare(first, marker.size(), marker) == 0 &&
            (contents[after_marker] == ' ' ||
             contents[after_marker] == '\t')) {
            const std::size_t quote = contents.find('"', first + marker.size());
            if (quote < end) {
                const std::size_t close = contents.find('"', quote + 1);
                if (close < end) {
                    *value_begin = quote + 1;
                    *value_end = close;
                    return true;
                }
            }
        }
        if (line_end == std::string::npos) {
            break;
        }
        line = line_end + 1;
    }
    return false;
}

inline std::string config_value(
    const std::string& contents, const char* key) {
    std::size_t begin = 0;
    std::size_t end = 0;
    return quoted_value_range(contents, key, &begin, &end)
               ? contents.substr(begin, end - begin)
               : "ausente";
}

inline bool set_config_value(
    std::string* contents, const char* key, const char* value) {
    std::size_t begin = 0;
    std::size_t end = 0;
    if (!quoted_value_range(*contents, key, &begin, &end)) {
        return false;
    }
    contents->replace(begin, end - begin, value);
    return true;
}

}  // namespace photorealism::aa_config
