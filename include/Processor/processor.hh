#ifndef __PROCESSOR_HH__
#define __PROCESSOR_HH__

#include "Sim/instruction.hh"
#include "Sim/mem_object.hh"
#include "Sim/request.hh"
#include "Sim/trace.hh"

#include <memory>
#include <deque>
#include <unordered_map>
#include <vector>

namespace CoreSystem
{
typedef uint64_t Addr;
typedef uint64_t Tick;

typedef Simulator::Instruction Instruction;
typedef Simulator::MemObject MemObject;
typedef Simulator::Request Request;
typedef Simulator::Trace Trace;

class Processor
{
  private:
    class Window // Instruction window
    {
      public:
        static const int IPC = 4; // instruction per cycle
        static const int DEPTH = 128; // window size
        // TODO, I currently hard-coded block_mask.
        Addr block_mask = 63;

      public:
        Window() {}
        bool isFull() { return num_issues == DEPTH; }
        bool isEmpty() { return num_issues == 0; } 
        void insert(Instruction &instr)
        {
            assert(num_issues <= DEPTH);
            assert(instr.opr == Instruction::Operation::LOAD);
            pending_instructions.push_back(instr);
            ++num_issues;
            head = (head + 1) % DEPTH;
        }

        int retire()
        {
            assert(num_issues <= DEPTH);
            if (isEmpty()) { return 0; }

            int retired = 0;
            while (num_issues > 0 && retired < IPC)
            {
                if (!pending_instructions[tail].ready_to_commit)
                {
                    break;
                }
                tail = (tail + 1) % DEPTH;
                num_issues--;
                retired++;
            }

            return retired;
	}

        auto commit()
        {
            return [this](Addr addr)
            {
                for (int i = 0; i < num_issues; i++)
                {
                    int index = (tail + i) % DEPTH;
                    Instruction &inst = pending_instructions[index];
                    if ((inst.opr == Instruction::Operation::LOAD) &&
                        (inst.target_addr & block_mask != addr))
                    {
                        continue;
                    }
                    inst.ready_to_commit = true;
                }

                return true;
            };
        }

      private:
        std::deque<Instruction> pending_instructions;
        int num_issues = 0;
        int head = 0;
        int tail = 0;
    };

    class Core
    {
      public:
        Core(int _id, const char* trace_file)
            : trace(trace_file),
              cycles(0),
              core_id(_id)
        {
            more_insts = trace.getInstruction(cur_inst);
            assert(more_insts);
        }

        void setDCache(MemObject* _d_cache) {d_cache = _d_cache;}

        void tick()
        {
            cycles++;

            window.retire();

            if (!more_insts) { return; }

            int inserted = 0;
            while (inserted < window.IPC && !window.isFull() && more_insts)
            {
                window.insert(cur_inst);
                inserted++;
                more_insts = trace.getInstruction(cur_inst);
            }
        }

        bool done()
        {
            return !more_insts && window.isEmpty();
        }

      private:
        Trace trace;

        Tick cycles;
        int core_id;

        Window window;

        bool more_insts;
        Instruction cur_inst;

        MemObject *d_cache;
        MemObject *i_cache;
    };

  public:
    Processor(std::vector<const char*> trace_lists) : cycles(0)
    {
        unsigned num_of_cores = trace_lists.size();
        for (int i = 0; i < num_of_cores; i++)
        {
            cores.emplace_back(new Core(i, trace_lists[i]));
        }
    }

    void setDCache(int core_id, MemObject *d_cache)
    {
        cores[core_id]->setDCache(d_cache);
    }

    void tick()
    {
        cycles++;
        for (auto &core : cores)
        {
            core->tick();
        }
    }

    bool done()
    {
        for (auto &core : cores)
        {
            if (!core->done())
            {
                return false;
            }
        }
        return true;
    }

  private:
    Tick cycles;
    std::vector<std::unique_ptr<Core>> cores;
};
}

#endif