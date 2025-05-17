#ifndef PROCSIM_HPP
#define PROCSIM_HPP

#include <cstdint>
#include <cstdio>
#include <iostream>
#include <queue>
#include <string>
#include <vector>
#include <algorithm>

#define DEFAULT_K0 3
#define DEFAULT_K1 2
#define DEFAULT_K2 1
#define DEFAULT_R 2
#define DEFAULT_F 4

#define VERBOSE 0
#define DEBUG 1
#define LOG 1

typedef struct _proc_inst_t
{
    int  tag            = -1;
    uint32_t instruction_address = 0;
    int32_t op_code     = -1;
    int32_t src_reg[2]  = {-1,-1};
    int32_t dest_reg    = -1;
    bool    fire        = 0;
    bool    retire      = 0;
    // int     res_st_num  = -1;
    int     dep_rs[2]   = {-1,-1};
    int     fu_unit     = -1;
    int     fetch_cnt   = -1;
    int     disp_cnt    = -1;
    int     sched_cnt   = -1;
    int     exec_cnt    = -1;
    int     update_cnt  = -1;
} proc_inst_t;

const proc_inst_t null_inst;

typedef struct _proc_stats_t
{
    float avg_inst_retired;
    float avg_inst_fired;
    float avg_disp_size;
    unsigned long max_disp_size;
    unsigned long retired_instruction;
    unsigned long cycle_count;
} proc_stats_t;

typedef struct _queue_state_t
{
    std::queue<proc_inst_t> Q;
    // uint32_t size;
    bool     ready;
} queue_state_t;

typedef struct _res_st_state_t
{
    proc_inst_t* buffer; // array to store instructions
    int          max;
    bool*        busy;   // array to mask filled entries
    bool*        ready;  // ready to receive data (extended to array for each entry) only true if allocated not immediately when busy->false
} res_st_state_t;

typedef struct _exec_instr_t
{
    int  tag = -1;
    int  res_st = -1;
    int  dest_reg = -1;
    bool to_bus = 0;
} exec_instr_t;

const exec_instr_t null_exec;

typedef struct _exec_state_t
{
    exec_instr_t* unit;
    int           max;
    bool*         busy;
    // bool          ready = 0;
} exec_state_t;

// typedef struct _bus_state_t
// {
//     std::vector<proc_inst_t> bus;
//     uint8_t max;
//     uint8_t size;
//     bool ready;
// } bus_state_t;

// bool comp_instr(proc_inst_t a, proc_inst_t b)
// {
//     return a.tag > b.tag;
// }

class procsim
{   
private:
    int clk_ctr   = 1;
    int tag_ctr   = 1;
    int print_ctr = 1;
    int  fetch_rate;
    // uint8_t  num_buses;
    // uint8_t  num_k0;
    // uint8_t  num_k1;
    // uint8_t  num_k2;
    // uint8_t  sched_buffer_size;
    queue_state_t*  fetch_state;
    queue_state_t*  disp_state;
    res_st_state_t* res_st_state;
    exec_state_t*   exec_state;
    exec_state_t    bus_state;
    std::vector<proc_inst_t> reorder_buffer;
public:
    procsim(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f) :
        fetch_rate(f)
        {   
            fetch_state = new queue_state_t;
            // fetch_state->size  = 0;
            fetch_state->ready = 1; // always fetch

            disp_state = new queue_state_t;
            // disp_state->size   = 0;
            disp_state->ready  = 0; // initially no data in fetch, can't dispatch

            res_st_state = new res_st_state_t;
            res_st_state->max   = 2*(k0 + k1 + k2);
            res_st_state->buffer = new proc_inst_t[res_st_state->max];
            res_st_state->busy   = new bool[res_st_state->max];
            res_st_state->ready  = new bool[res_st_state->max];
            for (int i = 0; i < res_st_state->max; i++)
            {
                res_st_state->buffer[i] = null_inst;
                res_st_state->busy[i]   = 0;
                res_st_state->ready[i]  = 0;
            }

            exec_state = new exec_state_t[3];
            exec_state[0].max   = k0;
            exec_state[0].unit  = new exec_instr_t[k0];
            exec_state[0].busy  = new bool[k0];
            exec_state[1].max   = k1;
            exec_state[1].unit  = new exec_instr_t[k1];
            exec_state[1].busy  = new bool[k1];
            exec_state[2].max   = k2;
            exec_state[2].unit  = new exec_instr_t[k2];
            exec_state[2].busy  = new bool[k2];
            for (int i=0;i<3;i++)
                for (int j=0;j<exec_state[i].max;j++)
                    exec_state[i].busy[j] = 0;

            bus_state.max   = r;
            bus_state.unit  = new exec_instr_t[r];
            bus_state.busy  = new bool[r];
            for (int i=0;i<bus_state.max;i++)
                    bus_state.busy[i] = 0;
            // bus_state.ready = 0;
        }
    ~procsim()
    {
        delete fetch_state;
        delete disp_state;
        delete res_st_state->buffer;
        delete res_st_state->busy;
        delete res_st_state->ready;
        delete res_st_state;
        delete exec_state->unit;
        delete exec_state->busy;
        delete exec_state;
        delete bus_state.unit;
        delete bus_state.busy;
    }

    int get_clk_ctr()    const  {return clk_ctr;}
    void inc_clk_ctr()          {clk_ctr++;}
    int get_tag_ctr()    const  {return tag_ctr;}
    int get_fetch_rate() const  {return fetch_rate;}
    // int get_num_buses() {return num_buses;}
    // int get_num_k0()    {return num_k0;}
    // int get_num_k1()    {return num_k1;}
    // int get_num_k2()    {return num_k2;}
    // int get_sched_buffer_size()     {return sched_buffer_size;}
    queue_state_t*  get_fetch_state() {return fetch_state;}
    queue_state_t*  get_disp_state()  {return disp_state;}
    res_st_state_t* get_res_st_state(){return res_st_state;}
    exec_state_t*   get_exec_state()  {return exec_state;}
    exec_state_t get_bus_state()      {return bus_state;}
    
    void fetch(int clk);
    void dispatch(int clk);
    void schedule(int clk);
    void execute(int clk);
    void bus(int clk);
    void update(int clk);
    void pipeline(int clk);

    

    // void sort_reorder_buffer()
    // {
    //     std::sort(reorder_buffer.begin(),reorder_buffer.end(),comp_instr);
    // }
    void print_instr(proc_inst_t i)
    {
        std::cout << "tag "     << i.tag;
        std::cout << " opcode "     << i.op_code;
        std::cout << " dest_reg "   << i.dest_reg;
        std::cout << " src_reg_1 "  << i.src_reg[0];
        std::cout << " src_reg_2 "  << i.src_reg[1];
        std::cout << " dep_rs_1 "   << i.dep_rs[0];
        std::cout << " dep_rs_2 "   << i.dep_rs[1];
        std::cout << " fire "   << i.fire;
        std::cout << " retire " << i.retire;
        std::cout << " FU "     << i.fu_unit;
        std::cout << " fetch_cnt " << i.fetch_cnt;
        std::cout << " disp_cnt " << i.disp_cnt;
        std::cout << " sched_cnt " << i.sched_cnt;
        std::cout << " exec_cnt " << i.exec_cnt;
        std::cout << " bus_cnt " << i.update_cnt;
        std::cout << std::endl;
    }

    int find_tag_reorder(int tag)
    {
        int i = 0;
        for (auto instr : reorder_buffer)
        {
            if (instr.tag == tag)
                return i;
            else
                i++;
        }
        return -1;
    }
};

bool read_instruction(proc_inst_t* p_inst);

void setup_proc(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f);
void run_proc(proc_stats_t* p_stats);
void complete_proc(proc_stats_t* p_stats);

#endif /* PROCSIM_HPP */
