#include "writer.hpp"

#include <cstddef>

namespace photorealism {
namespace config_writer {
namespace {

bool is_space(char value) {
    return value == ' ' || value == '\t' || value == '\r';
}

bool equals_ignore_case(
    const std::string& text,
    std::size_t begin,
    std::size_t end,
    const char* target) {
    std::size_t index = begin;
    const char* cursor = target;
    while (index < end && *cursor != '\0') {
        char left = text[index];
        char right = *cursor;
        left = left >= 'A' && left <= 'Z' ? static_cast<char>(left + 32) : left;
        right = right >= 'A' && right <= 'Z'
                    ? static_cast<char>(right + 32)
                    : right;
        if (left != right) {
            return false;
        }
        ++index;
        ++cursor;
    }
    return index == end && *cursor == '\0';
}

std::size_t line_end(const std::string& text, std::size_t begin) {
    const std::size_t found = text.find('\n', begin);
    return found == std::string::npos ? text.size() : found;
}

void trim_span(
    const std::string& text, std::size_t* begin, std::size_t* end) {
    while (*begin < *end && is_space(text[*begin])) {
        ++(*begin);
    }
    while (*end > *begin && is_space(text[*end - 1])) {
        --(*end);
    }
}

bool section_matches(
    const std::string& text,
    std::size_t begin,
    std::size_t end,
    const char* section,
    bool* is_header) {
    std::size_t start = begin;
    std::size_t stop = end;
    trim_span(text, &start, &stop);
    *is_header = stop > start && text[start] == '[' && text[stop - 1] == ']';
    if (!*is_header) {
        return false;
    }
    return equals_ignore_case(text, start + 1, stop - 1, section);
}

bool key_matches(
    const std::string& text,
    std::size_t begin,
    std::size_t end,
    const char* key,
    std::size_t* value_begin,
    std::size_t* value_end) {
    std::size_t start = begin;
    std::size_t stop = end;
    trim_span(text, &start, &stop);
    if (start >= stop || text[start] == '#' || text[start] == ';') {
        return false;
    }
    std::size_t separator = start;
    while (separator < stop && text[separator] != '=') {
        ++separator;
    }
    if (separator >= stop) {
        return false;
    }
    std::size_t name_begin = start;
    std::size_t name_end = separator;
    trim_span(text, &name_begin, &name_end);
    if (!equals_ignore_case(text, name_begin, name_end, key)) {
        return false;
    }
    *value_begin = separator + 1;
    *value_end = stop;
    trim_span(text, value_begin, value_end);
    return true;
}

void append_section(
    std::string* contents,
    const char* section,
    const char* key,
    const char* value) {
    if (!contents->empty() && contents->back() != '\n') {
        contents->push_back('\n');
    }
    contents->push_back('\n');
    contents->push_back('[');
    contents->append(section);
    contents->append("]\n");
    contents->append(key);
    contents->push_back('=');
    contents->append(value);
    contents->push_back('\n');
}

}

bool set_value(
    std::string* contents,
    const char* section,
    const char* key,
    const char* value) {
    if (contents == nullptr || section == nullptr || key == nullptr ||
        value == nullptr) {
        return false;
    }

    bool inside = false;
    std::size_t insert_at = std::string::npos;
    std::size_t cursor = 0;
    while (cursor <= contents->size()) {
        const std::size_t stop = line_end(*contents, cursor);
        bool is_header = false;
        const bool matched =
            section_matches(*contents, cursor, stop, section, &is_header);
        if (is_header && matched) {
            inside = true;
            insert_at = stop < contents->size() ? stop + 1 : contents->size();
        } else if (is_header && inside) {
            break;
        } else if (inside) {
            std::size_t value_begin = 0;
            std::size_t value_end = 0;
            if (key_matches(
                    *contents, cursor, stop, key, &value_begin, &value_end)) {
                contents->replace(
                    value_begin, value_end - value_begin, value);
                return true;
            }
        }
        if (stop >= contents->size()) {
            break;
        }
        cursor = stop + 1;
    }

    if (!inside) {
        append_section(contents, section, key, value);
        return true;
    }

    std::string line(key);
    line.push_back('=');
    line.append(value);
    line.push_back('\n');
    contents->insert(insert_at, line);
    return true;
}

}
}
