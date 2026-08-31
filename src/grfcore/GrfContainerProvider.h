#pragma once

#include "Container.h"

#include <string>

namespace grf {

// Dispatch by extension, mirroring GrfContainerProvider.Get:
//   .grf / .gpf  -> Container
//   .thor / .rgz -> format converters (not yet ported, M2)
//   anything else -> still parsed as a GRF container
Container openContainer(const std::string& path);

} // namespace grf