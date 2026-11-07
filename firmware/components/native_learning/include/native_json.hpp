#pragma once
#include <string>
#include <string_view>
#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#endif
#include "../vendor/json.hpp"
namespace wordcard {
template <class T> struct StudyAllocator : std::allocator<T> {
    using value_type = T;
    template <class U> struct rebind {
        using other = StudyAllocator<U>;
    };
    StudyAllocator() = default;
    template <class U> StudyAllocator(const StudyAllocator<U> &) {}
    T *allocate(size_t n) {
#ifdef ESP_PLATFORM
        if (n > SIZE_MAX / sizeof(T))
            throw std::bad_alloc();
        auto *p = static_cast<T *>(heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!p)
            throw std::bad_alloc();
        return p;
#else
        return std::allocator<T>::allocate(n);
#endif
    }
    void deallocate(T *p, size_t n) {
#ifdef ESP_PLATFORM
        (void)n;
        heap_caps_free(p);
#else
        std::allocator<T>::deallocate(p, n);
#endif
    }
};
using StudyJson = nlohmann::basic_json<nlohmann::ordered_map, std::vector, std::string, bool, int64_t,
                                       uint64_t, double, StudyAllocator>;
inline const StudyJson &field(const StudyJson &j, std::string_view k) {
    static const StudyJson nil;
    return j.is_object() && j.contains(k) ? j.at(k) : nil;
}
inline std::string str(const StudyJson &j) {
    return j.is_string()    ? j.get<std::string>()
           : j.is_number()  ? j.dump()
           : j.is_boolean() ? (j.get<bool>() ? "true" : "false")
                            : "";
}
inline std::string str(const StudyJson &j, const std::string &k) { return str(field(j, k)); }
inline int64_t num(const StudyJson &j, int64_t def = 0) { return j.is_number() ? j.get<int64_t>() : def; }
inline int64_t num(const StudyJson &j, const std::string &k, int64_t def = 0) {
    return num(field(j, k), def);
}
inline bool truth(const StudyJson &j) {
    return !j.is_null() && (!j.is_boolean() || j.get<bool>()) && (!j.is_number() || j.get<double>() != 0) &&
           (!j.is_string() || !j.get_ref<const std::string &>().empty());
}
inline bool flag(const StudyJson &j, const std::string &k) { return truth(field(j, k)); }
inline bool has(const StudyJson &a, const StudyJson &v) {
    return a.is_array() && std::find(a.begin(), a.end(), v) != a.end();
}
inline void addUnique(StudyJson &a, const StudyJson &v) {
    if (a.is_null())
        a = StudyJson::array();
    if (!has(a, v))
        a.push_back(v);
}
inline StudyJson keys(const StudyJson &j) {
    auto a = StudyJson::array();
    if (j.is_object())
        for (auto it = j.begin(); it != j.end(); ++it)
            a.push_back(it.key());
    return a;
}
inline void require(bool b, const char *message = "学习记录未通过校验") {
    if (!b)
        throw std::runtime_error(message);
}
inline void drop(StudyJson &a, const StudyJson &v) {
    if (a.is_array())
        a.erase(std::remove(a.begin(), a.end(), v), a.end());
}
inline bool actual(const StudyJson &r) { return flag(r, "correct") && !flag(r, "assisted"); }
} // namespace wordcard
