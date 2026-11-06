#include "../../src/spatial_audio/mat_pacing.h"
#include <iostream>
#include <stdexcept>
static void Check(bool b){if(!b)throw std::runtime_error("capture clock regression");}
int main(){try{
 constexpr int64_t hz=10000003,origin=12345678;
 for(int ppm : {-400,400}) {
  spatial_stream::EncoderClock c;int64_t target=0;
  for(int i=0;i<20;++i){const auto now=origin+int64_t(i)*hz/100;Check(c.deadline(now,hz,1,0,target));}
  int64_t firstTarget=0,lastCapture=0;
  constexpr uint64_t rounds=24ULL*3600*100;
  for(uint64_t n=0;n<rounds;++n){
   const auto nominal=int64_t(n/100)*hz+int64_t(n%100)*hz/100;
   const auto delta=nominal+nominal*ppm/1000000;
   const auto captured=origin+delta;
   const auto now=origin+hz/50+delta;
   Check(c.deadline(now,hz,2,captured,target));
   if(!n)firstTarget=target;
   Check(target-captured==hz/50);
   Check(c.within_budget(now,hz,2,target));
   Check(target-firstTarget==captured-origin);lastCapture=captured;
  }
  Check(!c.deadline(target,hz,2,lastCapture-1,target));
  const auto before=target;Check(c.deadline(target,hz,3,0,target));Check(target>before);
 }
 {
  spatial_stream::EncoderClock c;int64_t target=0,now=origin;
  for(int i=0;i<20;++i){now=origin+int64_t(i)*hz/100;Check(c.deadline(now,hz,1,0,target));}
  now=origin+hz*370/1000;
  for(int i=0;i<100;++i){
   const auto captured=origin+int64_t(i)*hz/100;
   Check(c.deadline(now,hz,2,captured,target));Check(c.within_budget(now,hz,2,target));
   if(now<target)now=target;
   now+=hz/500; // Dedicated codec can preserve all pre-roll inputs and catch up.
  }
  Check(c.source_settled());Check(now-(origin+99*hz/100)<hz/25);
  Check(!c.within_budget(now+hz/3,hz,2,target));
 }
 {
  spatial_stream::EncoderClock c;int64_t target=0;
  Check(c.deadline(origin+hz*370/1000,hz,2,origin,target));
  Check(c.within_budget(origin+hz*370/1000,hz,2,target));
  // A worker that never catches up must fail, not retain an arbitrary delay.
  Check(c.deadline(origin+hz*3370/1000,hz,2,origin+hz*3,target));
  Check(!c.within_budget(origin+hz*3370/1000,hz,2,target));
 }
 std::cout<<"PASS 24 simulated hours +/-400ppm, 20ms source lead, lossless bounded startup catch-up, backwards/stalled clocks rejected, drain cadence retained; nativeCapture=0\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
