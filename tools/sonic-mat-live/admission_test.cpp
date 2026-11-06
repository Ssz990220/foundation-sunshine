// Own-process ring pressure: no audio engine, hooks, vendor or endpoint.
#define NOMINMAX
#include "native_queue.h"
#include <memory>
#include <thread>
#include <iostream>
#include <stdexcept>
static void Check(bool b) { if(!b)throw std::runtime_error("native admission regression"); }
int main() {
 try {
  for(unsigned mode=0;mode<3;++mode) {
   auto input=std::make_unique<receiver::Shared>();auto output=std::make_unique<spatial_stream::Output>();uint64_t nonce[2]={123,456};
   sonic_live::initialize_native_queue(*input,*output,7,11,nonce);
   HANDLE data=CreateEventW(nullptr,FALSE,FALSE,nullptr);Check(data!=nullptr);
   auto packet=std::make_unique<receiver::Slot>();*packet={};packet->header.frames=480;packet->header.kind=1;packet->header.calls=1;
   packet->calls[0].id=1;packet->calls[0].type=2;packet->calls[0].flags=3;packet->calls[0].frames=480;packet->calls[0].pcm[0]=.25f;
   for(unsigned i=0;i<32;++i){packet->header.index=i;Check(sonic_live::enqueue_native(*input,*packet,data));}
   packet->header.index=32;Check(!sonic_live::enqueue_native(*input,*packet,data)); // old production path fails here
   std::thread consumer;
   if(mode<2)consumer=std::thread([&]{Sleep(25);if(mode==0){
     Check(input->slots[0].header.sequence==0 && input->slots[0].calls[0].pcm[0]==.25f);
     receiver::put(input->reader.read,1);
    }else receiver::fail(*input,receiver::cancelled);});
   DWORD waited=0;auto start=GetTickCount64();bool accepted=sonic_live::enqueue_native_wait(*input,*packet,data,200,&waited);
   if(consumer.joinable())consumer.join();
   Check(GetTickCount64()-start<500);
   if(mode==0){Check(accepted&&receiver::get(input->writer.write)==33&&waited>0);const auto &s=input->slots[0];Check(s.header.sequence==32&&s.header.index==32&&s.calls[0].pcm[0]==.25f&&receiver::checksum(s.header,s.calls)==s.header.checksum);}
   else {Check(!accepted&&receiver::get(input->writer.write)==32);if(mode==2)Check(waited>=200&&!receiver::get(input->common.fault));}
   CloseHandle(data);
  }
  std::cout<<"PASS full ring waits for space without packet loss/overwrite; cancellation and permanent-stall deadline honored; nativeCapture=0\n";return 0;
 } catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
