#include "FrameJson.hpp"
#include <iostream>
int main() {
    using namespace nyx;
    Snapshot s;s.complete=true;s.localActor=1;s.viewport={1000,500};s.timestampMs=1000;
    s.actors={{2,0,{{{100,0,0},{100,0,10}}}}};
    std::cout<<frameJson(s,{1000,500},1100)<<'\n';
    std::cout<<frameJson(s,{1000,500},1300)<<'\n';
    s.actors[0].skeleton.resize(65);
    std::cout<<frameJson(s,{1000,500},1100)<<'\n';
}
