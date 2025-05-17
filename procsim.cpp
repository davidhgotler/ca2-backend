#include "procsim.hpp"
#include <iostream>
#include <fstream>
#include <climits>

using namespace std;

// Globals
procsim* p_proccessor;
unsigned long fired_instructions;
unsigned long retired_instructions;
ofstream logfile;
string logfp = "procsim.log";

void procsim::fetch(int clk)
{
    if (clk==0)
    {
        if (fetch_state->ready)
        {
            if (VERBOSE)
                cout << "Fetched:" << endl;
            
            proc_inst_t* p_instr; // ptr for read_instruction
            // Read F instructions from trace file (memory) into i-cache
            bool read_success;
            for (int i = 0; i < fetch_rate; i++)
            {
                p_instr = new proc_inst_t; // allocate memory   
                read_success = read_instruction(p_instr); // Read from file

                // Tag instruction + timestamp (clock cycle)
                p_instr->tag = tag_ctr;
                p_instr->fetch_cnt = clk_ctr;

                // add to queue(fifo) to model i-cache - copies value
                fetch_state->Q.push(*p_instr);

                delete p_instr; // free memory

                if (VERBOSE)
                {
                    // print latest instr
                    cout << dec << fetch_state->Q.back().tag << " ";
                    cout << "INSTR ADDR = 0x" << hex << fetch_state->Q.back().instruction_address << ", ";
                    cout << "OPCODE = " << dec << fetch_state->Q.back().op_code << ", ";
                    cout << "DEST REG = " << fetch_state->Q.back().src_reg[0] << ", ";
                    cout << "SRC REG 0 = " << fetch_state->Q.back().src_reg[1] << ", ";
                    cout << "SRC REG 1 = " << fetch_state->Q.back().dest_reg << endl;
                }
                if (LOG)
                {
                    if (!logfile.is_open())
                        logfile.open(logfp);
                    logfile << clk_ctr << "\t" << "FETCHED" << "\t" << tag_ctr << endl;
                }

                if (!disp_state->ready)
                    disp_state->ready = 1;

                if (read_success)
                    tag_ctr++; // increment counter
            }
        }
    }
    // if (clk==2)
    // {
    //     // Set size variable at end of cycle for register functionality
    //     fetch_state->size = fetch_state->Q.size();
    // }
    
}
void procsim::dispatch(int clk)
{   
    
    // rising edge + 1st half
    if (clk==0)
    {
        if (disp_state->ready)
        {
            if (VERBOSE)
                cout << "Dispatched:" << endl;
            
            for (int i = 0; i < fetch_rate; i++)
            {
                // Move F instr's from i-buffer (fetch) to instr (dispatch) queue
                disp_state->Q.push(fetch_state->Q.front());
                disp_state->Q.back().disp_cnt = clk_ctr;
                fetch_state->Q.pop();

                if (VERBOSE)
                {
                    // print latest instr
                    cout << dec << disp_state->Q.back().tag << " ";
                    cout << "INSTR ADDR = 0x" << hex << disp_state->Q.back().instruction_address << ", ";
                    cout << "OPCODE = " << dec << disp_state->Q.back().op_code << ", ";
                    cout << "DEST REG = " << disp_state->Q.back().src_reg[0] << ", ";
                    cout << "SRC REG 0 = " << disp_state->Q.back().src_reg[1] << ", ";
                    cout << "SRC REG 1 = " << disp_state->Q.back().dest_reg << endl;
                }

                if (LOG)
                {
                    if (!logfile.is_open())
                        logfile.open(logfp);
                    logfile << clk_ctr << "\t" << "DISPATCHED" << "\t" << disp_state->Q.back().tag << endl;
                }
            }
        }
        
    }
    // first half (positive)
    if (clk==1)
    {
        if (disp_state->ready)
        {
            // allocate space in reservation station by setting ready = true for open entries
            for (int i = 0; i < res_st_state->max; i++)
            {
                if ((!res_st_state->busy[i]) || res_st_state->buffer[i].retire)
                    res_st_state->ready[i] = 1;
            }
        }
    }
    // second half (negative)
    // technically read from register file but we don't have any real values
    // if (clk==2)
    // {
    //     // Set size variable at end of cycle for register functionality
    //     disp_state->size = disp_state->Q.size();
    // }
}

void procsim::schedule(int clk)
{   
    if (clk==0)
    {
        if (disp_state->Q.size()>0)
        {
            if (VERBOSE)
                cout << "Scheduled:" << endl;

            // Load up to F instr's into reservation station
            int f = 0;
            for (int i = 0; i < res_st_state->max; i++)
            {
                // look for first F available table entries
                if (res_st_state->ready[i])
                {
                    res_st_state->buffer[i] = disp_state->Q.front();
                    disp_state->Q.pop();
                    res_st_state->buffer[i].sched_cnt = clk_ctr;
                    res_st_state->busy[i]  = 1;
                    res_st_state->ready[i] = 0;

                    if (VERBOSE)
                    {
                        // print latest instr
                        cout << dec << res_st_state->buffer[i].tag << " ";
                        cout << "INSTR ADDR = 0x" << hex << res_st_state->buffer[i].instruction_address << ", ";
                        cout << "OPCODE = " << dec << res_st_state->buffer[i].op_code << ", ";
                        cout << "DEST REG = " << res_st_state->buffer[i].src_reg[0] << ", ";
                        cout << "SRC REG 0 = " << res_st_state->buffer[i].src_reg[1] << ", ";
                        cout << "SRC REG 1 = " << res_st_state->buffer[i].dest_reg << endl;
                    }

                    if (LOG)
                    {
                        if (!logfile.is_open())
                            logfile.open(logfp);
                        logfile << clk_ctr << "\t" << "SCHEDULED" << "\t" << res_st_state->buffer[i].tag << endl;
                    }

                    f++;
                    if (f==fetch_rate) break;
                }
            // }
            // if (f==0)
            // {
            //     cout << "no room in res station clk " << clk_ctr << endl;
            // }
            }
        }
    }

    else if (clk==1)
    {
        // cout << clk << endl;
        // rename
        // Set any fu_type -1 -> 0
        for (int i = 0; i < res_st_state->max; i++)
            if (res_st_state->buffer[i].tag!=-1)
                res_st_state->buffer[i].op_code = (res_st_state->buffer[i].op_code==-1) ? 0 : res_st_state->buffer[i].op_code;
                
        // Set dependency variables 
        int d1;
        int d2;
        int dest;
        int sup_tag1 = -1;
        int rs1      = -1;
        int sup_tag2 = -1;
        int rs2      = -1;
        int src_tag;
        int dest_tag;
        // Search for RAW hazards
        int i = 0;
        int j = 0;
        for (i = 0; i < res_st_state->max; i++)
        {
            for (j = 0; j < i; j++)
            {   
                if (i!=j)
                {   

                    d1 = res_st_state->buffer[i].src_reg[0];
                    d2 = res_st_state->buffer[i].src_reg[1];
                    dest = res_st_state->buffer[j].dest_reg;
                    src_tag = res_st_state->buffer[i].tag;
                    dest_tag = res_st_state->buffer[j].tag;
                    if ((dest==d1) && (dest!=-1) && (res_st_state->buffer[j].dep_rs[0]==-1) && (!res_st_state->buffer[j].retire))
                    {                
                        if ((src_tag > dest_tag) && (dest_tag>sup_tag1))
                        {
                            // cout << "found dependency rs1 " << rs1 << endl;
                            sup_tag1 = res_st_state->buffer[j].tag;
                            rs1 = j;
                        }
                    }
                    if ((dest==d2) && (dest!=-1) && (res_st_state->buffer[j].dep_rs[1]==-1) && (!res_st_state->buffer[j].retire))
                    {                
                        if ((src_tag > dest_tag) && (dest_tag>sup_tag2))
                        {
                            // cout << "found dependency rs2 " << rs2 << endl;
                            sup_tag2 = res_st_state->buffer[j].tag;
                            rs2 = j;
                        }
                    }
                }
            }
            // Set latest dependency if found
            if (rs1 > -1)
            {
                res_st_state->buffer[j].dep_rs[0] = rs1;
                // cout << "dependency 1 set" << endl;
                // print_instr(res_st_state->buffer[i]);
                // print_instr(res_st_state->buffer[j]);
            }
            if (rs2 > -1)
            {
                res_st_state->buffer[j].dep_rs[1] = rs1;
                // cout << "dependency 2 set" << endl;
                // print_instr(res_st_state->buffer[i]);
                // print_instr(res_st_state->buffer[j]);
            }
        }

        // Check if dependencies have been retired and update them
        int r1,r2;
        for (int i=0; i< res_st_state->max; i++)
        {
            // dependencies (RS entry index)
            r1 = res_st_state->buffer[i].dep_rs[0];
            r2 = res_st_state->buffer[i].dep_rs[0];
            // cout << "entry " << i << " tag " << res_st_state->buffer[i].tag << " dest " << dest_reg << " dep regs " << d1 << " " << d2 << endl;
            if (res_st_state->buffer[r1].retire)
            {
                // cout << "clk " << clk_ctr << endl;
                // print_instr(res_st_state->buffer[i]);
                res_st_state->buffer[i].dep_rs[0] = -1;
                // print_instr(res_st_state->buffer[i]);
            }
            if (res_st_state->buffer[r2].retire)
            {
                // cout << "clk " << clk_ctr << endl;
                // print_instr(res_st_state->buffer[i]);
                res_st_state->buffer[i].dep_rs[1] = -1;
                // print_instr(res_st_state->buffer[i]);
            }
        }

        // mark indep. instrs to fire
        for (int k = 0; k<3; k++)
        {
            // check for any available units
            for (int u = 0; u < exec_state[k].max; u++)
            {
                if (!exec_state[k].busy[u] || exec_state[k].unit[u].to_bus)
                {
                    // find first instr w/ no dependencies
                    for (int i = 0; i < res_st_state->max; i++)
                    {
                        if ((res_st_state->buffer[i].op_code==k) && res_st_state->busy[i])
                        {
                            if ((!res_st_state->buffer[i].fire) && (res_st_state->buffer[i].dep_rs[0]==-1) && (res_st_state->buffer[i].dep_rs[1]==-1))
                            {
                                // mark to fire
                                res_st_state->buffer[i].fire = 1;
                                fired_instructions++;
                                // set target fu unit
                                res_st_state->buffer[i].fu_unit = u;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}

void procsim::execute(int clk)
{
    if (clk==0)
    {
        // cout << "ex start" << endl;
        int k;
        int u;
        // Move/copy instructions that are marked to fire, into units
        for (int i = 0; i < res_st_state->max; i++)
        {
            // Check if instr is ready to execute and has not been previously executed
            if (res_st_state->buffer[i].fire && (res_st_state->buffer[i].exec_cnt==-1))
            {
                // Copy to exec unit
                k = res_st_state->buffer[i].op_code;
                u = res_st_state->buffer[i].fu_unit;
                if ((k!=-1) && (u!=-1))
                {
                    exec_state[k].unit[u].tag = res_st_state->buffer[i].tag;
                    exec_state[k].unit[u].res_st = i;
                    exec_state[k].unit[u].dest_reg = res_st_state->buffer[i].dest_reg;
                    exec_state[k].busy[u] = 1;
                    res_st_state->buffer[i].exec_cnt = clk_ctr;
                }

                if (LOG)
                {
                    if (!logfile.is_open())
                        logfile.open(logfp);
                    logfile << clk_ctr << "\t" << "EXECUTED" << "\t" << exec_state[k].unit[u].tag << endl;
                }
            }
        }
    }
    if (clk==1)
    {
        // mark ops to move to bus in order of tags
        int min_tag;
        exec_instr_t* oldest;
        for (int r=0; r<bus_state.max; r++)
        {
            min_tag = INT_MAX;
            for (int k = 0; k < 3; k++)
            {
                for (int u = 0; u < exec_state[k].max; u++)
                {
                    if ((exec_state[k].unit[u].tag < min_tag) && exec_state[k].busy[u] && !exec_state[k].unit[u].to_bus)
                    {
                        min_tag = exec_state[k].unit[u].tag;
                        oldest = &exec_state[k].unit[u];
                    }
                }
            }
            if (min_tag < INT_MAX)
                oldest->to_bus = 1;   
        }
    }
    
}
void procsim::bus(int clk)
{
    if (clk==0)
    {
        // cout << "bus start" << endl;
        // Find oldest tag ready to be pushed to bus
        int min_tag;
        int kk;
        int uu;
        for (int r=0; r<bus_state.max; r++)
        {   
            min_tag = -1;
            for (int k = 0; k < 3; k++)
            {
                for (int u = 0; u<exec_state[k].max; u++)
                {
                    if (exec_state[k].unit[u].to_bus)
                    {
                        if (min_tag==-1)
                        {
                            min_tag = exec_state[k].unit[u].tag;
                            kk = k;
                            uu = u;
                        }
                        else if (min_tag > exec_state[k].unit[u].tag)
                        {
                            min_tag = exec_state[k].unit[u].tag;
                            kk = k;
                            uu = u;
                        }
                    }
                }
            }
            // Check if any instrs marked to bus
            int i;
            if (min_tag!=-1)
            {
                bus_state.unit[r] = exec_state[kk].unit[uu];
                bus_state.busy[r] = 1;

                i = bus_state.unit[r].res_st;
                res_st_state->buffer[i].update_cnt = clk_ctr;
                // bus_state.unit[r].complete = 1;
                exec_state[kk].unit[uu] = null_exec;
                exec_state[kk].busy[uu] = 0;                
                if (LOG)
                {
                    if (!logfile.is_open())
                        logfile.open(logfp);
                    logfile << clk_ctr << "\t" << "STATE UPDATE" << "\t" << bus_state.unit[r].tag << endl;
                }
            
            }
        }
    }
}


void procsim::update(int clk)
{
    if (clk==1)
    {
        int i;
        int tag;
        int fetch_cnt;
        int disp_cnt;
        int sched_cnt;
        int exec_cnt;
        int update_cnt;
        // Write to reg file (printout)
        for (int r = 0; r < bus_state.max; r++)
        {
            if (bus_state.busy[r])
            {
                i = bus_state.unit[r].res_st;
                reorder_buffer.push_back(res_st_state->buffer[i]);
            }
        }
        // sort_reorder_buffer();
        // if (print_ctr==reorder_buffer.front().tag)
        i = find_tag_reorder(print_ctr);

        if (i!=-1)
        {
            while (i!=-1)
            {
                // print in order
                if (DEBUG)
                {
                    tag        = reorder_buffer.at(i).tag;
                    fetch_cnt  = reorder_buffer.at(i).fetch_cnt;
                    disp_cnt   = reorder_buffer.at(i).disp_cnt;
                    sched_cnt  = reorder_buffer.at(i).sched_cnt;
                    exec_cnt   = reorder_buffer.at(i).exec_cnt;
                    update_cnt = reorder_buffer.at(i).update_cnt;
                    cout << tag << "\t" << fetch_cnt << "\t" << disp_cnt << "\t" << sched_cnt << "\t" << exec_cnt << "\t" << update_cnt << endl;
                }
                // increment print ctr and find next tag
                print_ctr++;
                i = find_tag_reorder(print_ctr);
            }
        }
    }
    else if (clk==2)
    {
        // reg file read by dispatch - no real values
        // update schedule buffer
        // 1. delete instr's marked to retire - Do this first to match output.
        for (int i = 0; i < res_st_state->max; i++)
        {
            if (res_st_state->buffer[i].retire)
            {
                // cout << "clk = " << clk_ctr << endl;
                // print_instr(res_st_state->buffer[i]);
                res_st_state->buffer[i] = null_inst;
                res_st_state->busy[i] = 0;
                res_st_state->ready[i] = 1;
            }
        }

        // 2. mark current bus instr's as complete/retire
        // and update dependencies of completed instr's on bus

        int entry;
        for (int r = 0; r < bus_state.max; r++)
        {
            if (bus_state.busy[r])
            {
                // reservation station entry that completed
                entry = bus_state.unit[r].res_st;
                // dest_reg = bus_state.unit[r].dest_reg;
                // Mark entry to retire
                res_st_state->buffer[entry].retire = 1;
                retired_instructions++;
            }
        }
    }
}

// Model pipeline registers
void procsim::pipeline(int clk)
{
    update(clk);
    bus(clk);
    execute(clk);
    schedule(clk);
    dispatch(clk);
    fetch(clk);
    // print_instr(res_st_state->buffer[4]);
}


/**
 * Subroutine for initializing the processor. You may add and initialize any global or heap
 * variables as needed.
 * XXX: You're responsible for completing this routine
 *
 * @r ROB size
 * @k0 Number of k0 FUs
 * @k1 Number of k1 FUs
 * @k2 Number of k2 FUs
 * @f Number of instructions to fetch
 */
void setup_proc(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f) 
{
    p_proccessor = new procsim(r,k0,k1,k2,f);
    if (LOG==1)
    {
        logfile.open(logfp);
        logfile << "CYCLE\tOPERATION\tINSTRUCTION" << endl;
    }
    if (DEBUG)
    {
        cout << "INST\tFETCH\tDISP\tSCHED\tEXEC\tSTATE" << endl;
    }
    
}

/**
 * Subroutine that simulates the processor.
 *   The processor should fetch instructions as appropriate, until all instructions have executed
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void run_proc(proc_stats_t* p_stats)
{
    bool incomplete = (p_proccessor->find_tag_reorder(p_proccessor->get_tag_ctr())==-1);
    uint64_t disp_size_sum = 0;
    p_stats->max_disp_size = 0;
    while (incomplete)
    {
        for (int c=0; c < 3; c++)
        {
            p_proccessor->pipeline(c);
            // cout << "clk " << p_proccessor->get_clk_ctr() << " " << c << endl;
        }
        p_stats->cycle_count = p_proccessor->get_clk_ctr();
        disp_size_sum += p_proccessor->get_disp_state()->Q.size();
        p_stats->avg_disp_size = disp_size_sum / p_stats->cycle_count;
        p_stats->avg_inst_fired = fired_instructions / p_stats->cycle_count;
        p_stats->retired_instruction = retired_instructions;
        p_stats->avg_inst_retired = retired_instructions / p_stats->cycle_count;
        if (p_proccessor->get_disp_state()->Q.size() > p_stats->max_disp_size)
            p_stats->max_disp_size = p_proccessor->get_disp_state()->Q.size();
        p_proccessor->inc_clk_ctr();
        incomplete = (p_proccessor->find_tag_reorder(p_proccessor->get_tag_ctr())==-1);
    }
}

/**
 * Subroutine for cleaning up any outstanding instructions and calculating overall statistics
 * such as average IPC, average fire rate etc.
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_proc(proc_stats_t *p_stats) 
{
    delete p_proccessor;
    logfile.close();
}
