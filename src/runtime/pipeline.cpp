#include "pixelforge/pipeline.h"

#include <utility>

namespace pixelforge {

Pipeline& Pipeline::add(std::string name, Filter filter) {
    filters_.push_back(NamedFilter{std::move(name), std::move(filter)});
    return *this;
}

Pipeline& Pipeline::add(NamedFilter filter) {
    filters_.push_back(std::move(filter));
    return *this;
}

Image Pipeline::run(const Image& input) const {
    Image current = input;
    for (const auto& f : filters_) {
        current = f.apply(current);
    }
    return current;
}

}  // namespace pixelforge
