#ifndef __PCMSIM_ARRAY_HH__
#define __PCMSIM_ARRAY_HH__

#include <cstdint>
#include <list>
#include <memory>
#include <vector>

#include "Sim/config.hh"

namespace PCMSim
{
class Array
{
    typedef uint64_t Tick;
    typedef Simulator::Config Config;

  public:
    Array(typename Config::Array_Level level_val,
          Config &cfg) : level(level_val), id(0)
    {
        cur_clk = 0;
        next_free = 0;

        int child_level = int(level_val) + 1;

        // Stop at bank level
        if (level == Config::Array_Level::Bank)
        {
            return;
        }
        assert(level != Config::Array_Level::Bank);

        int child_max = -1;

        if(level_val == Config::Array_Level::Channel)
        {
            child_max = cfg.num_of_ranks;
        }
        else if(level_val == Config::Array_Level::Rank)
        {
            child_max = cfg.num_of_banks;
        }
        assert(child_max != -1);

        for (int i = 0; i < child_max; ++i)
        {
            std::unique_ptr<Array> child =
            std::make_unique< Array>(typename Config::Array_Level(child_level), cfg);

            child->parent = this;
            child->id = i;
            children.push_back(std::move(child));
        }
    }

    void reInitialize()
    {
        cur_clk = 0;
        next_free = 0;

        for (auto &child : children)
        {
            child->reInitialize();
        }
    }

    typename Config::Array_Level level;
    int id;
    Array *parent;
    std::vector<std::unique_ptr<Array>>children;

    bool isFree(int target_rank, int target_bank)
    {
        // See if (1) bank is free; (2) channel is free.
        if (children[target_rank]->children[target_bank]->next_free <= cur_clk &&
            next_free <= cur_clk)
        {
            return true;
	}
        else
        {
            return false;
        }
    }

    // TODO, put this to laser file since this function is not a universal function.
    // Only consider the bank is free or not, help us to precisely track fine-grained bank
    // status.
    bool isBankFree(int target_rank, int target_bank)
    {
        if (children[target_rank]->children[target_bank]->next_free <= cur_clk)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    // TODO, put this to laser as well.
    void addBankLatency(int rank_id, int bank_id, unsigned bank_latency)
    {
        children[rank_id]->children[bank_id]->next_free = cur_clk + bank_latency;
    }


    void update(Tick clk)
    { 
        cur_clk = clk; 
    
        for (auto &child : children)
        {
            child->update(clk);
        }
    }

    void postAccess(int rank_id, int bank_id,
                    unsigned channel_latency,
                    unsigned bank_latency)
    {
        // Add channel latency
        next_free = cur_clk + channel_latency;

        // Add bank latency
        children[rank_id]->children[bank_id]->next_free = cur_clk + bank_latency;
    }

    
  private:
    Tick cur_clk;
    Tick next_free;
};
}

#endif

