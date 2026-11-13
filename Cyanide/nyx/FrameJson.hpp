#pragma once
#include "Projection.hpp"
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

namespace nyx {
// A bounded, address-free transport for a future native-to-JS bridge.
// Timestamp and current dimensions must come from the same monotonic clock
// and live viewport as the capture; this function cannot establish either.
inline std::string frameJson(const Snapshot& snapshot,Viewport viewport,std::uint64_t nowMs) {
    const Frame frame=buildFrame(snapshot,viewport,nowMs);
    const char* status="invalid";
    switch (frame.status) {
        case FrameStatus::ready: status="ready";break;
        case FrameStatus::incomplete: status="incomplete";break;
        case FrameStatus::stale: status="stale";break;
        case FrameStatus::viewportChanged: status="viewport_changed";break;
        case FrameStatus::invalid: break;
    }
    if (frame.lines.size()>4096) return "{\"status\":\"overflow\",\"lines\":[]}";
    std::ostringstream out;out.imbue(std::locale::classic());
    out<<std::setprecision(9)<<"{\"status\":\""<<status<<"\",\"width\":"<<viewport.width
       <<",\"height\":"<<viewport.height<<",\"lines\":[";
    bool first=true;
    for (const auto& line:frame.lines) {
        if (!first) out<<',';
        first=false;
        out<<'['<<line.a.x<<','<<line.a.y<<','<<line.b.x<<','<<line.b.y<<']';
    }
    out<<"]}";
    auto result=out.str();
    if (result.size()>262144) return "{\"status\":\"overflow\",\"lines\":[]}";
    return result;
}
} // namespace nyx
