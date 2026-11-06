// Own-process diagnostic fault injection; never opens an endpoint or vendor DLL.
#define NOMINMAX
#include "native_queue.h"
#include <memory>
#include <thread>
#include <iostream>
#include <stdexcept>
static void Check(bool x) { if(!x) throw std::runtime_error("worker failure regression"); }
static void Run(const wchar_t *path, unsigned mode) {
  HANDLE inMap=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(receiver::Shared),nullptr);
  HANDLE outMap=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(spatial_stream::Output),nullptr);
  auto *in=(receiver::Shared*)MapViewOfFile(inMap,FILE_MAP_ALL_ACCESS,0,0,sizeof(receiver::Shared));
  auto *out=(spatial_stream::Output*)MapViewOfFile(outMap,FILE_MAP_ALL_ACCESS,0,0,sizeof(spatial_stream::Output));
  Check(in && out); uint64_t nonce[2]={123,456};
  sonic_live::initialize_native_queue(*in,*out,7,11,nonce);out->format=spatial_stream::fakeFormat;
  HANDLE data=CreateEventW(nullptr,FALSE,FALSE,nullptr),cancel=CreateEventW(nullptr,TRUE,FALSE,nullptr);
  FILETIME created,exit,kernel,user;Check(GetProcessTimes(GetCurrentProcess(),&created,&exit,&kernel,&user));
  spatial_stream::Args args{sizeof(args),spatial_stream::streamVersion,inMap,data,cancel,GetCurrentProcess(),outMap,7,11,{123,456},(uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime,GetCurrentProcessId(),0,0,0};
  auto dll=LoadLibraryW(path);Check(dll!=nullptr);auto start=(spatial_stream::FakeStart)GetProcAddress(dll,"TestMatStreamReceiverFake");Check(start!=nullptr);
  spatial_stream::FakeConfig config{sizeof(config),spatial_stream::streamVersion,mode==0?1:0,mode==1?0:-1,mode==2?0:-1,mode==3?1u:0u,0};
  auto packet=std::make_unique<receiver::Slot>();*packet={};packet->header.frames=480;packet->header.kind=1;packet->header.calls=1;
  packet->calls[0].id=1;packet->calls[0].type=2;packet->calls[0].flags=receiver::createFlags(2);packet->calls[0].frames=480;
  if(mode==4)packet->calls[0].flags=receiver::updateFlags(2); // missing creation
  Check(sonic_live::enqueue_native(*in,*packet,data));
  const auto result=start(&args,&config);
  const spatial_stream::Failure expected[]={spatial_stream::Failure::initialize,spatial_stream::Failure::submit,spatial_stream::Failure::processResult,spatial_stream::Failure::processBytes,spatial_stream::Failure::objectContract};
  Check(result!=0 && out->header.workerFinished && out->header.status==-1 && out->header.error==receiver::codec && out->header.failure==expected[mode]);
  if(mode==3)Check(out->header.lastProduced==receiver::request+1);
  if(mode==2)Check(out->header.processHr==S_FALSE);
  FreeLibrary(dll);UnmapViewOfFile(in);UnmapViewOfFile(out);for(auto h:{inMap,outMap,data,cancel})CloseHandle(h);
}
int wmain(int argc,wchar_t **argv) {
  try { Check(argc==2);for(unsigned i=0;i<5;++i)Run(argv[1],i);
    std::cout<<"PASS generic fault 13 retains distinct initialize/submit/process HRESULT/byte-count/object-contract root cause; nativeCapture=0\n";return 0;
  } catch(const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
