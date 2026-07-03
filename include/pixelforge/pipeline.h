// pixelforge/pipeline.h -- chain of image filters.
//
// A Pipeline is an ordered sequence of Filter functions. Each Filter
// takes an Image and returns a new Image. Filters can be defined
// in-process (as lambdas / std::function) or loaded from external
// plugins (Phase 3). This keeps the runtime engine independent of
// any specific filter implementation.
#ifndef PIXELFORGE_PIPELINE_H
#define PIXELFORGE_PIPELINE_H

#include "pixelforge/export.h"
#include "pixelforge/image.h"

#include <functional>
#include <string>
#include <vector>

namespace pixelforge {

using Filter = std::function<Image(const Image&)>;

struct PIXELFORGE_API NamedFilter {
    std::string name;
    Filter      apply;
};

class PIXELFORGE_API Pipeline {
public:
    Pipeline& add(std::string name, Filter filter);
    Pipeline& add(NamedFilter filter);

    // Run all filters in order, returning the final image.
    Image run(const Image& input) const;

    std::size_t size() const noexcept { return filters_.size(); }
    bool        empty() const noexcept { return filters_.empty(); }

    const std::vector<NamedFilter>& filters() const noexcept { return filters_; }

private:
    std::vector<NamedFilter> filters_;
};

}  // namespace pixelforge

#endif  // PIXELFORGE_PIPELINE_H
