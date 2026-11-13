#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace nyx {
struct Vec3 { double x{}, y{}, z{}; };
struct Point { double x{}, y{}; };
struct Segment { Vec3 a, b; };
struct Line { Point a, b; std::uint64_t actor{}; };
struct Camera {
    Vec3 location;
    double pitch{}, yaw{}, roll{}, horizontalFov{90.0};
};
struct Viewport { unsigned width{}, height{}; };
struct Actor {
    std::uint64_t id{};
    int team{}; // Zero means unknown, not team zero.
    std::vector<Segment> skeleton;
};
struct Snapshot {
    std::uint64_t timestampMs{};
    std::uint64_t localActor{};
    int localTeam{};
    bool complete{};
    Viewport viewport;
    Camera camera;
    std::vector<Actor> actors;
};
enum class FrameStatus { ready, incomplete, stale, viewportChanged, invalid };
struct Frame { FrameStatus status{FrameStatus::invalid}; std::vector<Line> lines; };

inline bool finite(Vec3 v) {
    constexpr double limit = 1.0e12;
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z)
        && std::abs(v.x) < limit && std::abs(v.y) < limit && std::abs(v.z) < limit;
}
inline Vec3 mix(Vec3 a, Vec3 b, double t) {
    return {a.x + (b.x-a.x)*t, a.y + (b.y-a.y)*t, a.z + (b.z-a.z)*t};
}
inline double dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }

class Projector {
    Camera camera_;
    Viewport viewport_;
    Vec3 forward_, right_, up_;
    double focal_{};
    Vec3 cameraSpace(Vec3 p) const {
        const Vec3 d{p.x-camera_.location.x,p.y-camera_.location.y,p.z-camera_.location.z};
        return {dot(d,right_),dot(d,up_),dot(d,forward_)};
    }
    Point screen(Vec3 p) const {
        return {viewport_.width*0.5 + p.x*focal_/p.z,
                viewport_.height*0.5 - p.y*focal_/p.z};
    }
    static bool clipEdge(double p, double q, double& lo, double& hi) {
        // Round-off at an exact screen edge must not erase a visible bone.
        if (std::abs(q)<1.0e-7) q=0.0;
        if (p == 0.0) return q >= 0.0;
        const double r = q/p;
        if (p < 0.0) lo = std::max(lo,r); else hi = std::min(hi,r);
        return lo <= hi;
    }
    Projector(Camera camera, Viewport viewport) : camera_(camera), viewport_(viewport) {
        constexpr double radians = 3.14159265358979323846/180.0;
        const double p=camera.pitch*radians,y=camera.yaw*radians,r=camera.roll*radians;
        const double sp=std::sin(p),cp=std::cos(p),sy=std::sin(y),cy=std::cos(y);
        const double sr=std::sin(r),cr=std::cos(r);
        forward_={cp*cy,cp*sy,sp};
        right_={sr*sp*cy-cr*sy,sr*sp*sy+cr*cy,-sr*cp};
        up_={-(cr*sp*cy+sr*sy),cy*sr-cr*sp*sy,cr*cp};
        focal_=viewport.width*0.5/std::tan(camera.horizontalFov*radians*0.5);
    }
public:
    static std::optional<Projector> create(Camera c, Viewport v) {
        if (!finite(c.location) || !std::isfinite(c.pitch) || !std::isfinite(c.yaw)
            || !std::isfinite(c.roll) || !std::isfinite(c.horizontalFov)
            || c.horizontalFov < 10.0 || c.horizontalFov > 170.0
            || std::abs(c.pitch)>360.0 || std::abs(c.yaw)>360000.0
            || std::abs(c.roll)>360.0 || !v.width || !v.height
            || v.width>16384 || v.height>16384) return std::nullopt;
        return Projector(c,v);
    }
    std::optional<Line> project(Segment s, std::uint64_t actor=0) const {
        if (!finite(s.a) || !finite(s.b)) return std::nullopt;
        Vec3 a=cameraSpace(s.a), b=cameraSpace(s.b);
        constexpr double nearPlane=0.1;
        if (a.z<nearPlane && b.z<nearPlane) return std::nullopt;
        // Clip the segment, rather than dropping both ends when one is behind.
        if (a.z<nearPlane) { a=mix(a,b,(nearPlane-a.z)/(b.z-a.z)); a.z=nearPlane; }
        else if (b.z<nearPlane) { b=mix(b,a,(nearPlane-b.z)/(a.z-b.z)); b.z=nearPlane; }
        const Point p=screen(a), q=screen(b);
        if (!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(q.x)||!std::isfinite(q.y))
            return std::nullopt;
        const double dx=q.x-p.x,dy=q.y-p.y;
        double lo=0.0,hi=1.0;
        if (!clipEdge(-dx,p.x,lo,hi) || !clipEdge(dx,viewport_.width-p.x,lo,hi)
            || !clipEdge(-dy,p.y,lo,hi) || !clipEdge(dy,viewport_.height-p.y,lo,hi))
            return std::nullopt;
        auto bounded=[&](double t) {
            return Point{std::clamp(p.x+dx*t,0.0,double(viewport_.width)),
                         std::clamp(p.y+dy*t,0.0,double(viewport_.height))};
        };
        return Line{bounded(lo),bounded(hi),actor};
    }
};

inline Frame buildFrame(const Snapshot& s, Viewport current, std::uint64_t nowMs) {
    if (!s.complete || !s.localActor) return {FrameStatus::incomplete,{}};
    if (nowMs<s.timestampMs || nowMs-s.timestampMs>250) return {FrameStatus::stale,{}};
    if (s.viewport.width!=current.width || s.viewport.height!=current.height)
        return {FrameStatus::viewportChanged,{}};
    const auto projector=Projector::create(s.camera,current);
    if (!projector || s.actors.size()>1024) return {};
    // Reject oversized frames before producing any output.
    for (const auto& actor:s.actors) if (actor.skeleton.size()>64) return {};
    Frame result{FrameStatus::ready,{}};
    for (const auto& actor:s.actors) {
        if (!actor.id || actor.id==s.localActor || (s.localTeam>0 && actor.team==s.localTeam)) continue;
        for (const auto& segment:actor.skeleton)
            if (auto line=projector->project(segment,actor.id)) result.lines.push_back(*line);
    }
    return result;
}
} // namespace nyx
