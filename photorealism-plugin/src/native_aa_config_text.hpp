#pragma once

#include <cstddef>
#include <cstring>
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

// Leitor minimo do photorealism-plugin.cfg. O dinput8 roda no bootstrap,
// antes do dxgi, e nao pode usar o config.cpp que vive na outra DLL -- entao
// le so o que precisa: a secao que descreve o que escrever no config.cfg do
// jogo. Formato "[secao]" seguido de linhas "chave=valor", com "#" como
// comentario.
inline std::string plugin_config_value(
    const std::string& contents, const char* section, const char* key) {
    const std::string header = std::string("[") + section + "]";
    std::size_t line = 0;
    bool inside = false;
    while (line < contents.size()) {
        std::size_t first = line;
        while (first < contents.size() &&
               (contents[first] == ' ' || contents[first] == '\t')) {
            ++first;
        }
        const std::size_t line_end = contents.find('\n', first);
        std::size_t end =
            line_end == std::string::npos ? contents.size() : line_end;
        while (end > first &&
               (contents[end - 1] == '\r' || contents[end - 1] == ' ' ||
                contents[end - 1] == '\t')) {
            --end;
        }

        if (end > first && contents[first] != '#') {
            if (contents[first] == '[') {
                // Uma secao nova encerra a anterior: a chave nao existe aqui.
                if (inside) {
                    return "ausente";
                }
                inside = end - first == header.size() &&
                         contents.compare(first, header.size(), header) == 0;
            } else if (inside) {
                const std::size_t separator = contents.find('=', first);
                if (separator != std::string::npos && separator < end) {
                    std::size_t name_end = separator;
                    while (name_end > first &&
                           (contents[name_end - 1] == ' ' ||
                            contents[name_end - 1] == '\t')) {
                        --name_end;
                    }
                    const std::size_t name_size = name_end - first;
                    if (name_size == std::strlen(key) &&
                        contents.compare(first, name_size, key) == 0) {
                        std::size_t value = separator + 1;
                        while (value < end && (contents[value] == ' ' ||
                                               contents[value] == '\t')) {
                            ++value;
                        }
                        return contents.substr(value, end - value);
                    }
                }
            }
        }

        if (line_end == std::string::npos) {
            break;
        }
        line = line_end + 1;
    }
    return "ausente";
}

}  // namespace photorealism::aa_config
