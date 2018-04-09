#ifndef MEMORY_HIERARCHY
#define MEMORY_HIERARCHY

#include <unordered_set>

#include "inc_all.h"
#include "event_engine.h"
#include "memory_helper.h"
#include "event_engine.h"

using namespace std;

struct MemoryConfig;
struct MemoryEventData;
struct MemoryAccessInfo;

class CacheBlockBase;
class CacheBlockFactoryInterace;
class CacheSet;
class CRPolicyInterface;
class MemoryInterface;
class MemoryUnit;
class CacheUnit;
class MainMemory;
class MemoryStats;

/*********************************  Enums  ********************************/
enum CR_POLICY {
  LRU_POLICY,
  RANDOM_POLICY,
  POLICY_CNT
};
/**************************************************************************/

/*********************************  DTO   ********************************/

// cache unit
//  configuration
struct MemoryConfig {
  // both cache and main memory contains
  u8            proiority;
  u32           latency;

  // only cache contains
  u32           ways;
  u32           blk_size;
  u64           sets;
  CR_POLICY     policy_type;
};

struct MemoryEventData : public EventDataBase {
  u64 addr;
  u64 PC;

  MemoryEventData(u64 addr_, u64 PC_) : addr(addr_), PC(PC_) {};
  MemoryEventData(const MemoryAccessInfo &info);
};

struct MemoryAccessInfo {
  u64 addr;
  u64 PC;

  MemoryAccessInfo(u64 addr_, u64 PC_) : addr(addr_), PC(PC_) {};
  MemoryAccessInfo(const MemoryEventData &data);
};

/**************************************************************************/


/********************************  Objects ********************************/
class CacheBlockBase {
 protected:
  u64             _addr; 
  u32             _blk_size;       // length of the block
  u64             _tag;

  CacheBlockBase() {};

 public:
  CacheBlockBase(u64 addr, u32 blk_size, u64 tag):
      _addr(addr), _blk_size(blk_size), _tag(tag) {
        assert(is_power_of_two(blk_size));
      } 

  CacheBlockBase(const CacheBlockBase &other): 
      _addr(other._addr), _blk_size(other._blk_size), _tag(other._blk_size) {};

  virtual ~CacheBlockBase() {};

  inline u64 get_addr() {
    return _addr;
  }

  inline u32 get_blk_size() {
    return _blk_size;
  }

  inline u64 get_tag() {
    return _tag;
  }
};

class CacheBlockFactoryInterace{
 public:
  CacheBlockFactoryInterace() {};
  virtual ~CacheBlockFactoryInterace() {};
  virtual CacheBlockBase* create(u64 tag, u64 blk_size, const MemoryAccessInfo &info) = 0;
};

class CRPolicyInterface {
 protected:
  CacheBlockFactoryInterace *_factory;

 public:
  CRPolicyInterface(CacheBlockFactoryInterace *factory): _factory(factory) {};
  virtual ~CRPolicyInterface() {};
  virtual void on_hit(CacheSet *line, u32 pos, const MemoryAccessInfo &info) = 0;
  virtual void on_arrive(CacheSet *line, u64 tag, const MemoryAccessInfo &info) = 0;
};

class PolicyFactory {
 private:
  map<CR_POLICY, CRPolicyInterface*>      _shared_policies;
  vector<CRPolicyInterface*>              _policies;

 public:
  PolicyFactory() {};
  ~PolicyFactory();

  CRPolicyInterface* get_policy(CR_POLICY policy_type);

  // when policy use private data
  CRPolicyInterface* create_policy(CR_POLICY policy_type);
};


/**
 * Cache Set, all cache blokcs are stored in vector rather than queue
 * to give more exexpansibility
 */
class CacheSet {
 private:
  u32                               _ways;
  u32                               _blk_size;
  u32                               _sets;
  vector<CacheBlockBase *>          _blocks;
  CRPolicyInterface *               _cr_policy;
  // for verbose output
  string                            _set_tag;

  CacheSet() {};                        // forbid default constructor
  CacheSet(const CacheSet&) {};         // forbid copy constructor

  s32 find_pos_by_tag(u64 tag);

 public:
  CacheSet(u32 ways, u32 blk_size, u32 sets, CRPolicyInterface *policy);
  CacheSet(u32 ways, u32 blk_size, u32 sets, CRPolicyInterface *policy, const string &tag);
  ~CacheSet();

  inline const vector<CacheBlockBase *>& get_all_blocks() {
    return _blocks;
  }

  inline u32 get_ways() {
    return _ways;
  }

  inline u32 get_block_size() {
    return _blk_size;
  }

  u64 calulate_tag(u64 addr);

  // can evict an empty
  void evict_by_pos(u32 pos, CacheBlockBase *blk, bool is_delete = true);
  CacheBlockBase* get_block_by_pos(u32 pos);

  bool try_access_memory(const MemoryAccessInfo &info);
  void on_memory_arrive(const MemoryAccessInfo &info);

  void print_blocks(FILE* fs);
};

class MemoryInterface : public EventHandler {
 public:
  virtual bool try_access_memory(const MemoryAccessInfo &info) = 0;
  virtual void on_memory_arrive(const MemoryAccessInfo &info) = 0;
};

class MemoryUnit : public MemoryInterface {
 private:
  // for LLC there are multiple previous memory units
  vector<MemoryUnit*>     _prev_units;
  MemoryUnit *            _next_unit;
  
  // since there could be more than one previous memory units
  // we store all pending address to identify if need process a 
  // memory on arrive event
  unordered_set<u64>      _pending_refs;

  MemoryUnit() {};
  MemoryUnit(const MemoryUnit&) {};


 protected:
  // for event engine
  u32                     _latency;
  u8                      _priority;

  void proc(EventDataBase* data, EventType type);
  bool validate(EventType type);

 public:
  MemoryUnit(u32 latency, u8 priority) : _next_unit(NULL) ,
    _latency(latency), _priority(priority) {};

  virtual ~MemoryUnit(){};

  inline u32 get_latency() {
    return _latency;
  }

  inline u8 get_priority() {
    return _priority;
  }

  inline void add_prev(MemoryUnit *p) {
    _prev_units.push_back(p);
  }

  inline void set_next(MemoryUnit *n) {
    _next_unit = n;
  }
};

class CacheUnit: public MemoryUnit {
 private:
  // for memory
  u32                             _ways;
  u32                             _blk_size;
  u64                             _sets;
  CRPolicyInterface *             _cr_policy;
  vector<CacheSet*>               _cache_sets;

 public:
  
  CacheUnit(const MemoryConfig &config);
  ~CacheUnit ();

  inline u64 get_sets() {
    return _sets;
  }

  inline u32 get_blk_size() {
    return _blk_size;
  }

  inline CRPolicyInterface* get_policy() {
    return _cr_policy;
  }

  u64 get_set_no(u64 addr);

  bool try_access_memory(const MemoryAccessInfo &info);
  void on_memory_arrive(const MemoryAccessInfo &info);
};

/**
 * Main Memory
 * assume there is no MMU, no page fault
 */
class MainMemory: public MemoryUnit {
 public:
  MainMemory(const MemoryConfig &config);

  bool try_access_memory(const MemoryAccessInfo &info);
  void on_memory_arrive(const MemoryAccessInfo &info);
};

class MemoryStats {
 private:
  u64 _misses;
  u64 _hits;

 public:
  MemoryStats(): _misses(0), _hits(0) {};

  inline void increment_miss() {
    _misses++;
  }

  inline void increment_hit() {
    _hits++;
  }

  void display(FILE *stream);
  void clear();
};

class MemoryPipeLine {
 private:
  vector<MemoryUnit *>  _units;
  EventHandler*         _alu;
  MemoryUnit *          _head;

 public:
  
  MemoryPipeLine(vector<MemoryConfig> &configs, MemoryUnit *alu);
  ~MemoryPipeLine();
  bool try_access_memory(const MemoryAccessInfo &info);
};

/**************************************************************************/

/********************************  Singleton ******************************/

typedef Singleton<PolicyFactory> PolicyFactoryObj;
typedef Singleton <MemoryStats> MemoryStatsObj;

/**************************************************************************/

#endif
