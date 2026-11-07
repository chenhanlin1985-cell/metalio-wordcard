#pragma once
#include "native_json.hpp"
#include <functional>
#include <unordered_map>
namespace wordcard {
using J = StudyJson;
constexpr int64_t StudyDay = 86400000, StudyGap = 900000;
class NativeModel {
  public:
    explicit NativeModel(const J &data);
    J state;
    int64_t now = 0;
    const J &data() const { return data_; }
    const J &word(std::string id) const;
    const J &lesson(std::string id) const;
    const J &tasks(const J &s) const;
    const J &task(const J &s) const;
    J known(const J &x) const;
    J pending(const J &x) const;
    J memoryRow(const J &x, const std::string &id, const std::string &kind) const;
    J reviewItems(const J &x, const std::string &kind = "all", const std::string &mode = "due",
                  const J &ids = nullptr) const;
    J gap(const std::string &id, int64_t serial) const;
    J make(const J &ids, const J &source, int64_t serial, const std::string &mode = "learn") const;
    void applyMode(J &s, const std::string &mode) const;
    bool mutate(J &x, const std::string &method, const J &args = J::array());
    bool validate(const J &x) const;
    bool validateBatch(const J &s) const;
    bool validateResult(const J &r) const;
    static std::string normal(const std::string &s);
    static std::string dateKey(int64_t time);
    static std::string memoryLabel(const J &m);
    std::string sourceTitle(const J &s) const;
    bool complete(const J &x, const std::string &id) const;
    int unitCount(const J &x, int unit) const;
    J snapshot(const J &profile) const;
    static std::string nextLabel(const J &snap);

  private:
    const J &data_;
    std::unordered_map<std::string, const J *> words_, lessons_;
    J options(const std::string &id, const J &ids, int64_t serial) const;
    void checkBatch(J &s, bool unknown) const;
    bool nextBatch(J &s) const;
    void schedule(J &s, const J &q) const;
    bool addKnown(J &s, const std::string &id) const;
    J summarize(const J &s) const;
    void updateMemory(J &memory, const J &r) const;
    void recordDaily(const J &before, J &x) const;
    void advance(J &x) const;
};
} // namespace wordcard
