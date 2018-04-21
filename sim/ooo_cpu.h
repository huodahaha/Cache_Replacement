#ifndef CACHE_REPLACEMENT_OOO_CPU_H
#define CACHE_REPLACEMENT_OOO_CPU_H

#include "inc_all.h"
#include "event_engine.h"
#include "memory_hierarchy.h"
#include "trace_loader.h"

// on_memory_acess
// 重复的address会被
// poll

struct CPUEventData : public EventDataBase {
  u32 opcode;
  u64 PC;
  u64 destination_memory[NUM_INSTR_DESTINATIONS]; // output memory
  u64 source_memory[NUM_INSTR_SOURCES];
  CPUEventData(const TraceFormat &);
};

class MemoryConnecter {

};

class SequentialCPU : public EventHandler {
 private:
  const size_t             _id;
  const MultiTraceLoader * _multi_trace_loader;
  TraceFormat              _current_trace;
<<<<<<< HEAD
  const MemoryConnecter    _memomry_connecter;
  list<TraceFormat>        _instruction_list;
=======
  MemoryConnecter    _memomry_connecter;

  unordered_set<u64>       _pending_refs;
>>>>>>> 68057ce40fd3e36552042ab983ce8a835e34120b

  size_t get_operation_latency(const TraceFormat& trace) const;


 protected:
  void proc(u64 tick, EventDataBase* data, EventType type);

 public:
  // also initialize _memomry_connecter
  SequentialCPU(size_t id, MultiTraceLoaderObj multi_trace_loader):
      _id(id), _multi_trace_loader(multi_trace_loader.get_instance()) {}
};



//class OutOfOrderCPU : public MemoryUnit {
//
//};

#endif // CACHE_REPLACEMENT_OOO_CPU_H
