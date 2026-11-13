#include "Projection.hpp"
#include <cassert>
#include <limits>

static bool close(double a,double b) { return std::abs(a-b)<1e-6; }
int main() {
    using namespace nyx;
    const Viewport landscape{1000,500};
    auto p=Projector::create({},landscape);
    assert(p);
    auto point=p->project({{100,0,0},{100,0,0}});
    assert(point && close(point->a.x,500) && close(point->a.y,250));
    auto right=p->project({{100,100,0},{100,100,0}});
    assert(right && close(right->a.x,1000));
    auto up=p->project({{100,0,50},{100,0,50}});
    assert(up && close(up->a.y,0));
    assert(!p->project({{-100,0,0},{-10,0,0}}));
    // Crossing a screen edge must retain the visible part.
    auto clipped=p->project({{100,-200,0},{100,200,0}});
    assert(clipped && close(clipped->a.x,0) && close(clipped->b.x,1000));
    // Crossing the camera plane is not a whole-skeleton rejection.
    assert(p->project({{-1,0,0},{1,0,0}}));
    assert(!p->project({{1,std::numeric_limits<double>::quiet_NaN(),0},{1,0,0}}));
    Camera rotated;rotated.yaw=90;
    auto yaw=Projector::create(rotated,landscape)->project({{0,100,0},{0,100,0}});
    assert(yaw && close(yaw->a.x,500) && close(yaw->a.y,250));
    Camera narrow;narrow.horizontalFov=0;
    assert(!Projector::create(narrow,landscape));
    assert(!Projector::create({}, {0,0}));
    const Segment bone{{100,0,0},{100,0,10}};
    Snapshot snapshot;
    snapshot.complete=true;snapshot.localActor=1;snapshot.localTeam=5;
    snapshot.viewport=landscape;snapshot.timestampMs=1000;
    snapshot.actors={{1,5,{bone}},{2,5,{bone}},{3,0,{bone}},{4,6,{bone}}};
    auto frame=buildFrame(snapshot,landscape,1100);
    assert(frame.status==FrameStatus::ready && frame.lines.size()==2);
    assert(frame.lines[0].actor==3 && frame.lines[1].actor==4);
    assert(buildFrame(snapshot,landscape,1251).status==FrameStatus::stale);
    assert(buildFrame(snapshot,landscape,999).status==FrameStatus::stale);
    assert(buildFrame(snapshot,{500,1000},1100).status==FrameStatus::viewportChanged);
    snapshot.complete=false;
    assert(buildFrame(snapshot,landscape,1100).lines.empty());
    snapshot.complete=true;snapshot.actors.back().skeleton.resize(65);
    auto invalid=buildFrame(snapshot,landscape,1100);
    assert(invalid.status==FrameStatus::invalid && invalid.lines.empty());
}
