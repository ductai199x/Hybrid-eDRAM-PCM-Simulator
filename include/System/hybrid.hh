#ifndef __HYBRID_HH__
#define __HYBRID_HH__

#include <algorithm>
#include <random>

#include "System/mmu.hh"

namespace System
{
class Hybrid : public MMU
{
  protected:
    // Data structure for page information
    struct Page_Info
    {
        Addr page_id; // virtual page_id
        Addr re_alloc_page_id; // A page may be re-allocated to a different location (see below)
        
        Addr first_touch_instruction; // The first-touch instruction that brings in this page

        // Record physical location of a page. So far, a page may be in:
        //     (1) The fast-access region of PCM (pcm_near)
        //     (2) The slow-access region of PCM (pcm_far)
        //     (3) The fast-access region of DRAM (dram_near)
        //     (4) The slow-access region of DRAM (dram_far)
        bool in_pcm_near = false;
        bool in_pcm_far = false;
        bool in_dram_near = false;
        bool in_dram_far = false;

        uint64_t num_of_reads = 0; // Number of reads to the page
        uint64_t num_of_writes = 0; // Number of writes to the page

        // Number of phases the page hasn't been touched.
        unsigned num_of_phases_silent = 0;
    };
    // All the touched pages for each core.
    std::vector<std::unordered_map<Addr,Page_Info>> pages_by_cores;

    // Data structure for first-touch instruction (FTI)
    struct First_Touch_Instr_Info // Information of first-touch instruction
    {
        Addr eip;

        // A FTI can allocate a page to the following location.
        bool in_pcm_near = false;
        bool in_pcm_far = false;
        bool in_dram_near = false;
        bool in_dram_far = false;

        uint64_t num_of_reads = 0;
        uint64_t num_of_writes = 0;
    };
    // All the FTIs for each core
    std::vector<std::unordered_map<Addr,First_Touch_Instr_Info>> FTIs_by_cores;

    // Data structure for page migration (single page migration)
    struct Mig_Page
    {
        Addr page_id;

        bool done = false; // Is migration process done?

        bool pcm_far_to_pcm_near = false; // From far segment to near segment (PCM)
        bool pcm_near_to_pcm_far = false; // From near segment to far segment (PCM)

        bool pcm_near_to_dram_far = false; // From PCM near segment to DRAM far segment
        bool dram_far_to_pcm_near = false; // From DRAM far segment to PCM near segment

        bool dram_far_to_dram_near = false; // From far segment to near segment (DRAM)
        bool dram_near_to_dram_far = false; // From near segment to far segment (DRAM)

        // The following two migration type should also be supported in case of incorrect
        // intial allocation for top hot pages.
        bool pcm_far_to_dram_near = false; // From PCM far segment to DRAM near segment
        bool pcm_near_to_dram_near = false; // From PCM near segment to DRAM near segment

        // When page migration happens, a page should be read from the original node/segment 
        // then written to the target node/segment
        unsigned num_mig_reads_left;
        unsigned num_mig_writes_left;

        Addr ori_page_id;
        Addr target_page_id;
    };
    std::vector<Mig_Page> pages_to_migrate;

    // Memory sizes 
    std::vector<unsigned> mem_size_in_gb;

    // PageID helper, one for DRAM, one for PCM
    std::vector<PageIDHelper> page_id_helpers_by_technology;

    // A pool of free physical pages, one for DRAM, one for PCM
    std::vector<std::vector<Addr>> free_frame_pool_by_technology;
    
    const unsigned NUM_ROWS[int(Config::Memory_Node::MAX)];
    // A pool of free fast-access physical pages, one for DRAM, one for PCM
    const unsigned NUM_FAST_ACCESS_ROWS[int(Config::Memory_Node::MAX)];
    std::vector<std::vector<Addr>> free_fast_access_frame_pool_by_technology;

    // A pool of free slow-access physical pages, one for DRAM, one for PCM
    std::vector<std::vector<Addr>> free_slow_access_frame_pool_by_technology;

    // A pool of used physical pages, one for DRAM, one for PCM
    std::vector<std::unordered_map<Addr,bool>> used_frame_pool_by_technology;

  public:
    Hybrid(int num_of_cores, Config &dram_cfg, Config &pcm_cfg)
        : MMU(num_of_cores)
        , NUM_ROWS{dram_cfg.numRows(), pcm_cfg.numRows()}
        , NUM_FAST_ACCESS_ROWS{dram_cfg.numNearRows(), pcm_cfg.numNearRows()}
    {
        pages_by_cores.resize(num_of_cores);
        FTIs_by_cores.resize(num_of_cores);

        mem_size_in_gb.push_back(dram_cfg.sizeInGB());
        mem_size_in_gb.push_back(pcm_cfg.sizeInGB());

        page_id_helpers_by_technology.emplace_back(dram_cfg);
        page_id_helpers_by_technology.emplace_back(pcm_cfg);

        free_frame_pool_by_technology.resize(int(Config::Memory_Node::MAX));
        free_fast_access_frame_pool_by_technology.resize(int(Config::Memory_Node::MAX));
        free_slow_access_frame_pool_by_technology.resize(int(Config::Memory_Node::MAX));
        used_frame_pool_by_technology.resize(int(Config::Memory_Node::MAX));

        // Construct all available pages
        auto rng = std::default_random_engine {};
        for (int m = 0; m < int(Config::Memory_Node::MAX); m++)
        {
            for (int i = 0; i < mem_size_in_gb[m] * 1024 * 1024 / 4; i++)
            {
                // All available pages
                free_frame_pool_by_technology[m].push_back(i);

                // All free fast-access and slow-access physical pages
                auto &mem_addr_decoding_bits = 
                    page_id_helpers_by_technology[m].mem_addr_decoding_bits;
                std::vector<int> dec_addr;
                dec_addr.resize(mem_addr_decoding_bits.size());
                Decoder::decode(i << Mapper::va_page_shift,
                                mem_addr_decoding_bits,
                                dec_addr);

                int row_idx = page_id_helpers_by_technology[m].row_idx;
                if (dec_addr[row_idx] < NUM_FAST_ACCESS_ROWS[m])
                {
                    free_fast_access_frame_pool_by_technology[m].push_back(i);
                }
                else
                {
                    free_slow_access_frame_pool_by_technology[m].push_back(i);
                }
            }

            std::shuffle(std::begin(free_fast_access_frame_pool_by_technology[m]),
                         std::end(free_fast_access_frame_pool_by_technology[m]), rng);

            std::shuffle(std::begin(free_slow_access_frame_pool_by_technology[m]),
                         std::end(free_slow_access_frame_pool_by_technology[m]), rng);

            std::shuffle(std::begin(free_frame_pool_by_technology[m]),
                         std::end(free_frame_pool_by_technology[m]), rng);
            // std::cout << "Number of fast-access pages: "
            //           << free_fast_access_frame_pool_by_technology[m].size() << "\n";
            // std::cout << "Number of slow-access pages: "
            //           << free_slow_access_frame_pool_by_technology[m].size() << "\n";
            // std::cout << "Total number of pages: "
            //           << free_frame_pool_by_technology[m].size() << "\n\n";
        }
        // exit(0);
    }

    // Default: randomly map a virtual page to DRAM or PCM (segment is not considered)
    void va2pa(Request &req) override
    {
        int core_id = req.core_id;

        Addr va = req.addr;
        Addr virtual_page_id = va >> Mapper::va_page_shift;

        auto &pages = pages_by_cores[core_id];

        if (auto p_iter = pages.find(virtual_page_id);
                p_iter != pages.end())
        {
            Addr page_id = p_iter->second.re_alloc_page_id;
            req.addr = (page_id << Mapper::va_page_shift) |
                       (va & Mapper::va_page_mask);

            if (req.req_type == Request::Request_Type::READ) { (p_iter->second.num_of_reads)++; }
            if (req.req_type == Request::Request_Type::WRITE) { (p_iter->second.num_of_writes)++; }
        }
        else
        {
            // Keep track of page information
            bool in_pcm_near = false;
            bool in_pcm_far = false;
            bool in_dram_near = false;
            bool in_dram_far = false;

            static int total_mem_size = mem_size_in_gb[int(Config::Memory_Node::DRAM)] + 
                                        mem_size_in_gb[int(Config::Memory_Node::PCM)];

            static std::default_random_engine e{};
            static std::uniform_int_distribution<int> d_tech{1, total_mem_size};

            // Randomly determine which technology to be mapped
            int chosen_technology = 0;
            if (int random_num = d_tech(e);
                random_num <= mem_size_in_gb[int(Config::Memory_Node::DRAM)])
            {
                chosen_technology = int(Config::Memory_Node::DRAM);
                // std::cout << "Mapped to DRAM \n";
            }
            else
            {
                chosen_technology = int(Config::Memory_Node::PCM);
                // std::cout << "Mapped to PCM \n";
            }

            // Randomly determine near or far segment to be mapped
            static std::uniform_int_distribution<int> d_pcm_region{1, NUM_ROWS[int(Config::Memory_Node::PCM)]};
            static std::uniform_int_distribution<int> d_dram_region{1, NUM_ROWS[int(Config::Memory_Node::DRAM)]};

            auto *free_frames = &(free_slow_access_frame_pool_by_technology[chosen_technology]);
            if (chosen_technology == int(Config::Memory_Node::PCM))
            {
                if (int random_num = d_pcm_region(e);
                        random_num <= NUM_FAST_ACCESS_ROWS[int(Config::Memory_Node::PCM)])
                {
                    free_frames = &(free_fast_access_frame_pool_by_technology[int(Config::Memory_Node::PCM)]);
                    in_pcm_near = true;
                }
                else
                {
                    in_pcm_far = true;
                }
            }
	    else if (chosen_technology == int(Config::Memory_Node::DRAM))
            {
                if (int random_num = d_dram_region(e);
                        random_num <= NUM_FAST_ACCESS_ROWS[int(Config::Memory_Node::DRAM)])
                {
                    free_frames = &(free_fast_access_frame_pool_by_technology[int(Config::Memory_Node::DRAM)]);
                    in_dram_near = true;
                }
                else
                {
                    in_dram_far = true;
		}
            }

            auto &used_frames = used_frame_pool_by_technology[chosen_technology];
            // std::cout << "Size of free frames: " << free_frames.size() << "\n";
            // std::cout << "Size of used frames: " << used_frames.size() << "\n";

            // Choose a free frame
            Addr free_frame = *(free_frames->begin());
            // for (int i = 0; i < 11; i++)
            // {
            //     std::cout << free_frames[i] << "\n";
            // }

            free_frames->erase(free_frames->begin());
            used_frames.insert({free_frame, true});

            // std::cout << "\nSize of free frames: " << free_frames.size() << "\n";
            // std::cout << "Size of used frames: " << used_frames.size() << "\n";
            // for (int i = 0; i < 10; i++)
            // {
            //     std::cout << free_frames[i] << "\n";
            // }

            req.addr = (free_frame << Mapper::va_page_shift) |
                       (va & Mapper::va_page_mask);
            // std::cout << "\nPhysical address: " << req.addr << "\n";
            // Insert the page
            uint64_t num_of_reads = 0;
            uint64_t num_of_writes = 0;
            if (req.req_type == Request::Request_Type::READ) { num_of_reads++; }
            if (req.req_type == Request::Request_Type::WRITE) { num_of_writes++; }

            pages.insert({virtual_page_id, {virtual_page_id, 
                                            free_frame,
                                            req.eip,
                                            in_pcm_near,
                                            in_pcm_far,
                                            in_dram_near,
                                            in_dram_far,
                                            num_of_reads,
                                            num_of_writes}});
            // exit(0);
        }
    }

    int memoryNode(Request &req) override
    {
        Addr page_id = req.addr >> Mapper::va_page_shift;
        // std::cout << "Page ID: " << page_id << "\n";
        for (int m = 0; m < int(Config::Memory_Node::MAX); m++)
        {
            auto &used_frames = used_frame_pool_by_technology[m];

            if (auto p_iter = used_frames.find(page_id);
                    p_iter != used_frames.end())
            {
                return m;
            }
        }

        std::cerr << "Invalid Page ID.\n";
        exit(0);
    }

    void registerStats(Simulator::Stats &stats)
    {
        uint64_t num_pages = 0;
        uint64_t num_pages_in_near_DRAM = 0;
        uint64_t num_pages_in_far_DRAM = 0;
        uint64_t num_pages_in_near_PCM = 0;
        uint64_t num_pages_in_far_PCM = 0;

        for (auto &pages : pages_by_cores)
        {
            num_pages += pages.size();

            for (auto [v_page, page_info] : pages)
            {
                if (page_info.in_pcm_near == true) { num_pages_in_near_PCM++; }
                if (page_info.in_pcm_far == true) { num_pages_in_far_PCM++; }
                if (page_info.in_dram_near == true) { num_pages_in_near_DRAM++; }
                if (page_info.in_dram_far == true) { num_pages_in_far_DRAM++; }
            }
        }

        std::string prin = "MMU_Total_Pages = " + std::to_string(num_pages);
        stats.registerStats(prin);

        prin = "MMU_Pages_in_near_DRAM = " + std::to_string(num_pages_in_near_DRAM);
        stats.registerStats(prin);
        prin = "MMU_Pages_in_far_DRAM = " + std::to_string(num_pages_in_far_DRAM);
        stats.registerStats(prin);
	
        prin = "MMU_Pages_in_near_PCM = " + std::to_string(num_pages_in_near_PCM);
        stats.registerStats(prin);
        prin = "MMU_Pages_in_far_PCM = " + std::to_string(num_pages_in_far_PCM);
        stats.registerStats(prin);
    }
};
}

#endif
