#include "processor.h"

namespace Processor
{
Processor::Processor(std::vector<const char*>trace_lists)
    : cycles(0)
{
    unsigned num_of_cores = trace_lists.size();
    for (int i = 0; i < num_of_cores; i++)
    {
        cores.emplace_back(new Core(i, trace_lists[i]));
    }
    assert(cores.size());
}

void Processor::tick()
{
    cycles++;
    for (unsigned int i = 0 ; i < cores.size() ; ++i) 
    {
        // Careful, cores[i] is a unique pointer.
        Core* core = cores[i].get();
        core->tick();
    }
}

bool Processor::done()
{
    for (unsigned int i = 0 ; i < cores.size() ; ++i)
    {
        // Careful, cores[i] is a unique pointer.
        Core* core = cores[i].get();
        if (!core->done())
        {
            return false;
        }
    }

    return true;
}

Core::Core(int _core_id, const char* trace_file)
    : cycles(0),
      core_id(_core_id),
      trace(trace_file),
      retired(0)
{
    more_insts = trace.getInstruction(cur_inst);
    assert(more_insts); // The trace file is empty.
}

// TODO, only dispatch instruction from window when it is full.
// (1) useful for dependency check, for now, all dependencies are ignored.
// (2) useful when calculating ICache misses. Load all the instructions to ROB,
// this is similar to Sniper Sim.
// TODO, study more on Sniper-Sim and implement above
void Core::tick()
{
    cycles++;

    // Tick the cache

    // Re-tire instructions
    retired += window.retire();

    // Do not proceed if there is not any instruction left
    if (!more_insts)
    {
        return;
    }

    // Insert instruction to window
    int inserted = 0;
    while (inserted < window.ipc && !window.is_full() && more_insts)
    {
        window.insert(true, cur_inst.target_addr);
        
	inserted++;
        // Get next instruction
        more_insts = trace.getInstruction(cur_inst);
    }
}

bool Core::done()
{
    return !more_insts && window.is_empty();
}

// Window
bool Window::is_full()
{
    return load == depth;
}

bool Window::is_empty()
{
    return load == 0;
}

void Window::insert(bool ready, Addr addr)
{
    assert(load <= depth);

    ready_list.at(head) = ready;
    addr_list.at(head) = addr;

    head = (head + 1) % depth;
    load++;
}

long Window::retire()
{
    assert(load <= depth);

    if (load == 0) return 0;

    int retired = 0;
    while (load > 0 && retired < ipc)
    {
        if (!ready_list.at(tail))
        {
            break;
        }
	tail = (tail + 1) % depth;
        load--;
        retired++;
    }

    return retired;
}

void Window::set_ready(Addr addr, int mask)
{
    if (load == 0) return;

    // TODO, better use shift (get the block address)
    for (int i = 0; i < load; i++) {
        int index = (tail + i) % depth;
        if ((addr_list.at(index) & mask) != (addr & mask))
            continue;
        ready_list.at(index) = true;
    }
}

CPUTraceReader::CPUTraceReader(const char *trace)
    : file(trace)
{
    if (!file.good()) 
    {
        std::cerr << "Bad trace file: " << trace << std::endl;
        exit(1);
    }
}

bool
CPUTraceReader::getInstruction(Instruction &inst)
{
    std::string line;
    getline(file, line);
    if (file.eof()) 
    {
        // Comment section: in the future, we may need to run the same
        // trace file for multiple times.
        // file.clear();
        // file.seekg(0, file.beg);
        // getline(file, line);
        file.close();
        return false; // tmp implementation, only run once
    }

    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::copy(std::istream_iterator<std::string>(iss),
              std::istream_iterator<std::string>(),
              std::back_inserter(tokens));
    
    assert(tokens.size() != 0);
    // TODO, hard-coded for now
    if (tokens.size() == 2)
    {
        inst.op_type = tokens[0];
        inst.EIP = std::stoull(tokens[1], NULL, 10);
        inst.target_addr = Addr(0) - 1;
    }
    else
    {
        inst.op_type = tokens[0];
        inst.EIP = std::stoull(tokens[1], NULL, 10);
        inst.target_addr = std::stoull(tokens[2], NULL, 10);
    }

    return true;
}
}