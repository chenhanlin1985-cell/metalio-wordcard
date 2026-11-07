// FSRS-6 equations ported from ts-fsrs@6.0.0 (MIT); see LICENSE-ts-fsrs.txt.
#include "vocabulary.hpp"
#include <algorithm>
#include <cmath>
namespace wordcard {
namespace {
constexpr double w[] = {.212,   1.2931, 2.3065, 8.2956, 6.4133, .8334, 3.0194,
                        .001,   1.8722, .1666,  .796,   1.4835, .0614, .2629,
                        1.6483, .6014,  1.8729, .5425,  .0912,  .0658, .1542};
double round8(double n) {
    return std::round(n * 1e8) / 1e8;
}
double bound(double x, double a, double b) {
    return std::min(std::max(x, a), b);
}
double init_d(int g) {
    return round8(w[4] - std::exp((g - 1) * w[5]) + 1);
}
double factor() {
    return round8(std::exp(-std::log(.9) / w[20]) - 1);
}
} // namespace
VJ vocabulary_fsrs(const VJ &m, int g, int t) {
    require((g == 1 || g == 3) && t >= 0, "无效复习评分");
    if (m.is_null())
        return VJ{{"difficulty", bound(init_d(g), 1, 10)}, {"stability", w[g - 1]}};
    const double d = m.at("difficulty").get<double>(), s = m.at("stability").get<double>();
    require(std::isfinite(d) && std::isfinite(s) && d >= 1 && d <= 10 && s >= .001 && s <= 36500,
            "记忆状态损坏");
    const auto r = round8(std::pow(1 + factor() * t / s, -w[20]));
    double ns;
    if (t == 0) {
        double growth = std::pow(s, -w[19]) * std::exp(w[17] * (g - 3 + w[18]));
        if (g >= 2)
            growth = std::max(growth, 1.);
        ns = round8(bound(s * growth, .001, 36500));
    } else if (g == 1) {
        const auto failed = round8(bound(
            w[11] * std::pow(d, -w[12]) * (std::pow(s + 1, w[13]) - 1) * std::exp((1 - r) * w[14]),
            .001, 36500));
        ns = bound(round8(s / std::exp(w[17] * w[18])), .001, failed);
    } else
        ns = round8(bound(s * (1 + std::exp(w[8]) * (11 - d) * std::pow(s, -w[9]) *
                                       (std::exp((1 - r) * w[10]) - 1)),
                          .001, 36500));
    const auto nd =
        bound(round8(w[7] * init_d(4) + (1 - w[7]) * (d + round8(-w[6] * (g - 3) * (10 - d) / 9))),
              1, 10);
    return VJ{{"difficulty", nd}, {"stability", ns}};
}
int vocabulary_interval(const VJ &m) {
    return std::max(1, int(std::round(m.at("stability").get<double>() / factor() *
                                      (std::pow(.9, -1 / w[20]) - 1))));
}
} // namespace wordcard
