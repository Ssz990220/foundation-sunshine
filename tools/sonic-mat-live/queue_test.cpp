// Generated own-process plugin objects -> real IPC receiver's FAKE worker.
// This test never loads a vendor component or attaches to an audio engine.
#define NOMINMAX
#include "plugin_bridge.h"
#include "native_queue.h"
#include <thread>
#include <atomic>
#include <iostream>
static void
Check(bool b) {
  if (!b) throw std::runtime_error("Plugin IPC integration assertion");
}
int
wmain(int argc, wchar_t **argv) {
  try {
    Check(argc == 2 || (argc == 3 && (std::wstring(argv[2]) == L"--bed-7.1.4" || std::wstring(argv[2]) == L"--paced-stress")));
    const bool paced = argc == 3 && std::wstring(argv[2]) == L"--paced-stress";
    const bool bed = argc == 3;
    const unsigned rounds = paced ? 9000 : 1300;
    const unsigned lanes = bed ? 14u : 1u;
    auto type = [&](unsigned n) { return bed && n < 12 ? 1u << (n + 1) : 1u; };
    HANDLE inputMap = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(receiver::Shared), nullptr);
    HANDLE outputMap = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(spatial_stream::Output), nullptr);
    auto *input = (receiver::Shared *) MapViewOfFile(inputMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(receiver::Shared));
    auto *output = (spatial_stream::Output *) MapViewOfFile(outputMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(spatial_stream::Output));
    Check(input && output);
    uint64_t nonce[2] = { 1234, 5678 };
    sonic_live::initialize_native_queue(*input, *output, 7, 11, nonce);
    output->format = spatial_stream::fakeFormat;
    HANDLE data = CreateEventW(nullptr, FALSE, FALSE, nullptr), cancel = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    FILETIME created {}, exit {}, kernel {}, user {};
    Check(GetProcessTimes(GetCurrentProcess(), &created, &exit, &kernel, &user) != FALSE);
    spatial_stream::Args args { sizeof(args), spatial_stream::streamVersion, inputMap, data, cancel, GetCurrentProcess(), outputMap, 7, 11, { nonce[0], nonce[1] }, (uint64_t(created.dwHighDateTime) << 32) | created.dwLowDateTime, GetCurrentProcessId(), 0, 0, 0 };
    auto dll = LoadLibraryW(argv[1]);
    Check(dll != nullptr);
    auto start = (spatial_stream::FakeStart) GetProcAddress(dll, "TestMatStreamReceiverFake");
    Check(start != nullptr);
    spatial_stream::FakeConfig config { sizeof(config), spatial_stream::streamVersion, 0, -1, -1, 0, paced ? 10u : 0u };
    std::atomic<DWORD> result = STILL_ACTIVE;
    std::thread worker([&] { result = start(&args, &config); });
    std::atomic<bool> failed = false;
    uint64_t blocks = 0, source = 0;
    std::string error;
    std::thread reader([&] {try{
        auto slot=std::make_unique<spatial_stream::OutputSlot>();auto deadline=GetTickCount64()+(paced?105000:15000);
        for(;;){Check(GetTickCount64()<deadline);auto status=spatial_stream::next(*output,*slot,blocks);
            if(status==spatial_stream::Poll::pending){Sleep(1);continue;}Check(status!=spatial_stream::Poll::failed);if(status==spatial_stream::Poll::ended)break;
            auto& b=slot->header;Check(b.format==spatial_stream::fakeFormat&&b.sampleStart==blocks*480);uint32_t phase,index,count;memcpy(&phase,slot->payload,4);memcpy(&index,slot->payload+4,4);memcpy(&count,slot->payload+8,4);
            Check(phase==b.phase&&index==b.sourceIndex);if(phase==2){Check(index==source&&count==lanes);
                for(unsigned n=0;n<lanes;++n){auto* p=slot->payload+12+n*1940;uint32_t id,flags;memcpy(&id,p,4);memcpy(&flags,p+4,4);Check(id==n+1&&flags==receiver::updateFlags(type(n)));
                    float xyz[3];memcpy(xyz,p+8,12);Check(xyz[0]==(type(n)==1?float(source%100)/100.f:0.f)&&xyz[1]==0&&xyz[2]==0);
                    const float expected=(float(source%31)/100.f+float(n)*.01f)*(.3f*.5f);float pcm[480];memcpy(pcm,p+20,sizeof(pcm));for(auto f:pcm)Check(f==expected);}
                ++source;
            }
        if(paced && blocks % 500 == 499) Sleep(120);
        ++blocks;
        }
    }catch(const std::exception& e){error=e.what();failed=true;SetEvent(cancel);} });
    sonic_live::PluginBridge bridge;
    auto pass = std::make_unique<sonic_live::Pass>();
    *pass = {};
    pass->renderer = 7;
    pass->deviceFrames = 480;
    pass->rate = 48000;
    pass->count = 1;
    auto &o = pass->objects[0];
    o.host = 99;
    o.id = 42;
    o.type = 1;
    o.frames = 480;
    o.result = S_OK;
    o.volume = .3f;
    o.gain = .5f;
    auto emit = [&](const receiver::Slot &packet) {Check(packet.header.calls==lanes);for(unsigned n=0;n<lanes;++n)Check(packet.calls[n].type==type(n));
        return sonic_live::enqueue_native_wait(*input,packet,data);
     };
    try {
      const auto began = receiver::qpc(), hz = receiver::hz();
      for (unsigned n = 0; n < rounds; ++n) {
        const auto nominal = int64_t(n / 100) * hz + int64_t(n % 100) * hz / 100;
        const auto due = began + nominal - nominal * 4000 / 1000000;
        if (paced) {
          while(receiver::qpc() < due) { receiver::put(input->writer.heartbeat, receiver::qpc()); Sleep(1); }
        }
        pass->sequence = n;
        pass->firstQpc = paced ? due : receiver::qpc();
        pass->lastQpc = pass->firstQpc;
        o.xyz[0] = float(n % 100) / 100.f;
        for (auto &f : o.pcm) f = float(n % 31) / 100.f;
        q4::Quantum q;
        q.domain = 1;
        q.renderer = 7;
        q.frames = 480;
        q.fullSpanLayoutReady = true;
        for (unsigned lane = 0; lane < lanes; ++lane) {
          q4::Object object;
          object.resultSeen = true;
          object.offset = 0;
          object.pcm.assign(480, float(n % 31) / 100.f + float(lane) * .01f);
          object.state.generation = 99 + lane;
          object.state.volumeKnown = 1;
          object.state.positionState = type(lane) == 1 ? position_state::Updated : position_state::StaticChannel;
          object.state.audio.objectId = 42 + lane;
          object.state.audio.type = type(lane);
          object.state.audio.x = o.xyz[0];
          object.state.audio.volume = .3f;
          object.state.audio.callGain = .5f;
          q.objects.push_back(std::move(object));
        }
        bridge.push(q, pass->firstQpc, pass->lastQpc, 48000, emit);
        if (n % 37 == 0) {
          q.objects.clear();
          q.producedBytes = 3840;
          bridge.push(q, pass->firstQpc, pass->lastQpc, 48000, emit);
        }
      }
      bridge.finish(emit);
    }
    catch (...) {
      SetEvent(cancel);
      worker.join();
      reader.join();
      throw;
    }
    worker.join();
    reader.join();
    Check(!failed && result == 0 && source == rounds && blocks == rounds + 50 && bridge.outputOnlyPasses() == (rounds + 36)/37 && output->header.eofSeen && output->header.fakeCallsVerified == (rounds+21) * lanes && output->header.sourceSamples == uint64_t(rounds) * 480 * lanes && output->header.vendorLoads == 0 && output->header.hostGuardPassed == 0);
    Check(output->header.failure == spatial_stream::Failure::none && output->header.localHighWater <= 8);
    std::cout << "PASS rounds=" << rounds << " paced=" << paced << " maxLateMs=" << double(output->header.maximumLatenessQpc)*1000/receiver::hz() << " object bridge -> actual IPC FAKE codec worker: lanes=" << lanes << ", owned type/PCM/XYZ/gain exact, consecutive blocks and complete EOS; nativeCapture=0\n";
    FreeLibrary(dll);
    UnmapViewOfFile(input);
    UnmapViewOfFile(output);
    for (auto h : { inputMap, outputMap, data, cancel }) CloseHandle(h);
    return 0;
  }
  catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
