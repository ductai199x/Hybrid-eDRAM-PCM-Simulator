#ifndef __SIM_CONFIG_HH__
#define __SIM_CONFIG_HH__

#include <array>
#include <cassert>
#include <fstream>
#include <iostream>
#include <math.h>
#include <sstream>
#include <string>
#include <vector>

namespace Simulator
{
class Config
{
  public:
    // TODO, should be vector since multi-core should be supported as well.
    std::string workload; // Name of the running workload

    // Processor configuration
    float on_chip_frequency;
    float off_chip_frequency;

    // Cache configuration
    unsigned block_size;
    bool cache_detailed; // Shall we care about latency generated by cache?

    enum class Cache_Level
    {
        L1I, L1D, L2, L3, eDRAM, MAX
    };

    struct Cache_Info
    {
        int assoc;
        unsigned size;
        bool write_only;
        unsigned num_mshrs;
        unsigned num_wb_entries;
        unsigned tag_lookup_latency;
    };
    std::array<Cache_Info, int(Cache_Level::MAX)> caches;
    void extractCacheInfo(Cache_Level level, std::vector<std::string> &tokens);

    // System Configuration
    bool trained_mmu = false;
    double perc_re_alloc = 0.0;

    // Memory Controller
    std::string mem_controller_type = "N/A";
  
    // Charge-pump info (stage-wise charging may apply)
    std::string charge_pump_info = "N/A";

    enum Charge_Pump_Opr : int
    {
        SET, RESET, READ, MAX
    };
    struct Charging_Stage
    {
        float voltage;
        unsigned nclks_charge_or_discharge;
    };
    unsigned num_stages = 0;
    std::vector<Charging_Stage> charging_lookaside_buffer[int(Charge_Pump_Opr::MAX)];
    void parseChargePumpInfo(const std::string &fname);

    // Running average power should always below RAPL? (Default no)
    bool power_limit_enabled = false;
    // OrderID should never exceed back-logging threshold? (Default no)
    bool starv_free_enabled = false;
    double RAPL; // running average power limit
    int THB; // back-logging threshold

    // PCM Array Architecture
    unsigned num_of_word_lines_per_tile;
    unsigned num_of_bit_lines_per_tile;
    unsigned num_of_tiles;
    unsigned num_of_parts;

    unsigned num_of_banks;
    unsigned num_of_ranks;
    unsigned num_of_channels;

    // Timing and energy parameters
    unsigned tRCD;
    unsigned tData;
    unsigned tWL;
    unsigned tWR;
    unsigned tCL;

    double pj_bit_rd;
    double pj_bit_set;
    double pj_bit_reset;

//  public:
    Config(const std::string &cfg_file);

    void parse(const std::string &fname);

    // We ignore the effect of Tile and Partition for now.
    enum class Array_Level : int
    {
        Channel, Rank, Bank, MAX
    };

    enum class Decoding : int
    {
        // Address mapping: bank-interleaving
        // Rank, Row, Col, Bank, Channel, Cache_Line, MAX

        // Channel-interleaving + Bank-interleaving + Partition-interleaving
	// When PLP is enable, you can do this:
        // Rank, Row, Col, Partition, Bank, Channel, Cache_Line, MAX
        Rank, Partition, Row, Col, Bank, Channel, Cache_Line, MAX
    };
    std::vector<int> mem_addr_decoding_bits;
    void genMemAddrDecodingBits();

    // For PCM
    unsigned sizeOfPCMInGB()
    {
        unsigned long long num_of_word_lines_per_bank = num_of_word_lines_per_tile *
                                                        num_of_parts;
        
	unsigned long long num_of_byte_lines_per_bank = num_of_bit_lines_per_tile /
                                                        8 *
                                                        num_of_tiles;

        unsigned long long size = num_of_word_lines_per_bank *
                                  num_of_byte_lines_per_bank *
                                  num_of_banks * num_of_ranks * num_of_channels /
                                  1024 / 1024 / 1024;
        return size;
    }
};
}
#endif
