#include "GrfContainerProvider.h"

namespace grf {

namespace {
std::string extensionLower(const std::string& path)
{
    auto dot = path.find_last_of('.');
    auto slash = path.find_last_of("/\\");
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return {};
    return toLowerAscii(path.substr(dot));
}
} // namespace

Container openContainer(const std::string& path)
{
    std::string ext = extensionLower(path);
    if (ext == ".thor" || ext == ".thm" || ext == ".thz")
        return Container::openThor(path);
    if (ext == ".rgz")
        return Container::openRgz(path);
    return Container::open(path);
}

} // namespace grf