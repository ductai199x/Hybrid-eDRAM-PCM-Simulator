#ifndef __CONFIG_HH__
#define __CONFIG_HH__

#include <array>
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace Configuration
{
class Config
{
  public:
    std::string workload; // Name of the running workload

    // Processor configuration
    float on_chip_frequency;
    float off_chip_frequency;

    // Cache configuration
    unsigned blkSize;
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

    // Memory Controller
    std::string mem_controller_family = "N/A";
    std::string mem_controller_type = "N/A";
   
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
  
  public:
    Config(const std::string &cfg_file);

    void parse(const std::string &fname);

    // We ignore the effect of Tile and Partition for now.
    enum class Level : int
    {
        Channel, Rank, Bank, MAX
    };

    enum class Decoding : int
    {
        // Address mapping: bank-interleaving
        // Rank, Row, Col, Bank, Channel, Cache_Line, MAX

        // Channel-interleaving + Bank-interleaving + Partition-interleaving
	// When PLP is not enable, you can do this:
	// Rank, Partition, Row, Col, Bank, Channel, Cache_Line, MAX
        Rank, Row, Col, Partition, Bank, Channel, Cache_Line, MAX
    };

    // For PCM
    unsigned sizeInGB()
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
