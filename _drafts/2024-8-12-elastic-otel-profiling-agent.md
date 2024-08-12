项目迁移到[这里](https://github.com/open-telemetry/opentelemetry-ebpf-profiler) (贡献给opentelemetry了）
## ubuntu上运行
version(`./otel-profiling-agent -version
1.0.0`)  
- 启动桌面
`startx`, 原因是[这个](https://github.com/open-telemetry/opentelemetry-ebpf-profiler/issues/14)
- 启动devfiler 
`./devfiler-appimage-x86_64.AppImage`  
- 启动agent
`./otel-profiling-agent -collection-agent=127.0.0.1:11000 -disable-tls`

## 查看加载的eBPF程序
```
96: perf_event  name unwind_stop  tag fae114b82570e3a6  gpl
        loaded_at 2024-08-12T06:53:10+0000  uid 0
        xlated 1912B  jited 1106B  memlock 4096B  map_ids 4136,4127,4161,4126,4154,4156,4120,4122
        pids otel-profiling-(248974)
97: perf_event  name unwind_native  tag 8cb52dc37b580f55  gpl
        loaded_at 2024-08-12T06:53:10+0000  uid 0
        xlated 30104B  jited 18718B  memlock 32768B  map_ids 4136,4127,4142,4139,4117,4152,4165,4160,4141,4124,4148,4158,4119,4132,4130,4146,4150,4166,4143,4144,4137
        pids otel-profiling-(248974)
98: perf_event  name unwind_hotspot  tag 1b16d67b1b30cdfd  gpl
        loaded_at 2024-08-12T06:53:11+0000  uid 0
        xlated 24912B  jited 16377B  memlock 28672B  map_ids 4136,4163,4127,4143,4144,4137
        pids otel-profiling-(248974)
99: perf_event  name unwind_perl  tag a1e3600b9c3977d8  gpl
        loaded_at 2024-08-12T06:53:11+0000  uid 0
        xlated 23536B  jited 15344B  memlock 24576B  map_ids 4136,4134,4127,4120,4137
        pids otel-profiling-(248974)
100: perf_event  name unwind_php  tag 6263ab26eab28b4d  gpl
        loaded_at 2024-08-12T06:53:11+0000  uid 0
        xlated 24656B  jited 15579B  memlock 28672B  map_ids 4136,4155,4127,4153,4143,4137
        pids otel-profiling-(248974)
101: perf_event  name unwind_python  tag d080953241eeff47  gpl
        loaded_at 2024-08-12T06:53:11+0000  uid 0
        xlated 27064B  jited 15995B  memlock 28672B  map_ids 4136,4167,4127,4120,4137
        pids otel-profiling-(248974)
102: perf_event  name unwind_ruby  tag 9f4542b44e98c357  gpl
        loaded_at 2024-08-12T06:53:11+0000  uid 0
        xlated 28528B  jited 18819B  memlock 28672B  map_ids 4136,4125,4127,4137
        pids otel-profiling-(248974)
103: perf_event  name unwind_v8  tag aaf69c0349b0d4a4  gpl
        loaded_at 2024-08-12T06:53:11+0000  uid 0
        xlated 27352B  jited 17008B  memlock 28672B  map_ids 4136,4135,4127,4143,4144,4137
        pids otel-profiling-(248974)
104: tracepoint  name tracepoint__sch  tag 5b45f73c7a5ca381  gpl
        loaded_at 2024-08-12T06:53:11+0000  uid 0
        xlated 832B  jited 464B  memlock 4096B  map_ids 4161,4143,4126,4127,4154,4156
        pids otel-profiling-(248974)
105: perf_event  name native_tracer_e  tag 269b8bf14315d6ed  gpl
        loaded_at 2024-08-12T06:53:11+0000  uid 0
        xlated 3104B  jited 2206B  memlock 4096B  map_ids 4136,4127,4133,4143,4161,4126,4144,4137,4154,4156
        pids otel-profiling-(248974)
```

## ebpf代码
hook点 perf_event
- perf_event/unwind_native
- perf_event/native_tracer_entry
- native_stack_trace.ebpf.c

tracer/tracer.go  
freg 默认20，一秒20次  
```
// AttachTracer attaches the main tracer entry point to the perf interrupt events. The tracer
// entry point is always the native tracer. The native tracer will determine when to invoke the
// interpreter tracers based on address range information.
func (t *Tracer) AttachTracer(sampleFreq int) error {
    tracerProg, ok := t.ebpfProgs["native_tracer_entry"]
    if !ok {
        return fmt.Errorf("entry program is not available")
    }

    perfAttribute := new(perf.Attr)
    perfAttribute.SetSampleFreq(uint64(sampleFreq))
    if err := perf.CPUClock.Configure(perfAttribute); err != nil {
        return fmt.Errorf("failed to configure software perf event: %v", err)
    }

    onlineCPUIDs, err := hostcpu.ParseCPUCoreIDs(hostcpu.CPUOnlinePath)
    if err != nil {
        return fmt.Errorf("failed to get online CPUs: %v", err)
    }

    events := t.perfEntrypoints.WLock()
    defer t.perfEntrypoints.WUnlock(&events)
    for _, id := range onlineCPUIDs {
        perfEvent, err := perf.Open(perfAttribute, perf.AllThreads, id, nil)
        if err != nil {
            return fmt.Errorf("failed to attach to perf event on CPU %d: %v", id, err)
        }
        if err := perfEvent.SetBPF(uint32(tracerProg.FD())); err != nil {
            return fmt.Errorf("failed to attach eBPF program to perf event: %v", err)
        }
        *events = append(*events, perfEvent)
    }
    return nil
}
```

## go
/processmanager/ebpf/ebpf.go
UpdatePidPageMappingInfo  
