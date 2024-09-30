#include "mem/MAA/SPD.hh"
#include "base/trace.hh"
#include "mem/MAA/MAA.hh"
#include "base/types.hh"
#include "debug/SPD.hh"
#include "sim/cur_tick.hh"
#include <cassert>
#include <cstring>

#ifndef TRACING_ON
#define TRACING_ON 1
#endif

namespace gem5 {

///////////////
//
// SPD
//
///////////////
Cycles SPD::getDataLatency(int num_accesses) {
    int min_busy_port = 0;
    Tick min_busy_until = read_port_busy_until[0];
    for (int i = 0; i < num_read_ports; i++) {
        if (read_port_busy_until[i] < min_busy_until) {
            min_busy_until = read_port_busy_until[i];
            min_busy_port = i;
        }
    }
    if (read_port_busy_until[min_busy_port] < curTick()) {
        read_port_busy_until[min_busy_port] = curTick();
    }
    read_port_busy_until[min_busy_port] += maa->getCyclesToTicks(Cycles(read_latency * num_accesses));
    DPRINTF(SPD, "%s: read_port_busy_until[%d] = %lu\n", __func__, min_busy_port, read_port_busy_until[min_busy_port]);
    panic_if(read_port_busy_until[min_busy_port] < curTick(),
             "Scheduled read at %lu, but current tick is %lu!\n",
             read_port_busy_until[min_busy_port], curTick());
    return maa->getTicksToCycles(read_port_busy_until[min_busy_port] - curTick());
}
Cycles SPD::setDataLatency(int num_accesses) {
    int min_busy_port = 0;
    Tick min_busy_until = write_port_busy_until[0];
    for (int i = 0; i < num_write_ports; i++) {
        if (write_port_busy_until[i] < min_busy_until) {
            min_busy_until = write_port_busy_until[i];
            min_busy_port = i;
        }
    }
    if (write_port_busy_until[min_busy_port] < curTick()) {
        write_port_busy_until[min_busy_port] = curTick();
    }
    write_port_busy_until[min_busy_port] += maa->getCyclesToTicks(Cycles(write_latency * num_accesses));
    panic_if(write_port_busy_until[min_busy_port] < curTick(),
             "Scheduled write at %lu, but current tick is %lu!\n",
             write_port_busy_until[min_busy_port], curTick());
    DPRINTF(SPD, "%s: write_port_busy_until[%d] = %lu\n", __func__, min_busy_port, write_port_busy_until[min_busy_port]);
    return maa->getTicksToCycles(write_port_busy_until[min_busy_port] - curTick());
}
uint16_t SPD::getReady(int tile_id) {
    check_tile_id<uint32_t>(tile_id);
    return tiles_ready[tile_id];
}
void SPD::setReady(int tile_id) {
    check_tile_id<uint32_t>(tile_id);
    tiles_ready[tile_id] = 1;
}
void SPD::unsetReady(int tile_id) {
    check_tile_id<uint32_t>(tile_id);
    tiles_ready[tile_id] = 0;
}
uint16_t SPD::getSize(int tile_id) {
    check_tile_id<uint32_t>(tile_id);
    panic_if(getReady(tile_id) == 0,
             "Trying to get size of an uninitialized tile[%d]!\n",
             tile_id);
    return tiles_size[tile_id];
}
void SPD::setSize(int tile_id, uint16_t size) {
    assert((0 <= tile_id) && (tile_id < num_tiles));
    tiles_size[tile_id] = size;
}
SPD::SPD(MAA *_maa,
         unsigned int _num_tiles,
         unsigned int _num_tile_elements,
         Cycles _read_latency,
         Cycles _write_latency,
         int _num_read_ports,
         int _num_write_ports)
    : num_tiles(_num_tiles),
      num_tile_elements(_num_tile_elements),
      read_latency(_read_latency),
      write_latency(_write_latency),
      num_read_ports(_num_read_ports),
      num_write_ports(_num_write_ports),
      maa(_maa) {

    tiles_data = new uint8_t[num_tiles * num_tile_elements * sizeof(uint32_t)];
    tiles_ready = new uint16_t[num_tiles];
    tiles_size = new uint16_t[num_tiles];
    for (int i = 0; i < num_tiles; i++) {
        tiles_ready[i] = 1;
        tiles_size[i] = 0;
    }
    memset(tiles_data, 0, num_tiles * num_tile_elements * sizeof(uint32_t));
    read_port_busy_until = new Tick[num_read_ports];
    write_port_busy_until = new Tick[num_write_ports];
    for (int i = 0; i < num_read_ports; i++) {
        read_port_busy_until[i] = curTick();
    }
    for (int i = 0; i < num_write_ports; i++) {
        write_port_busy_until[i] = curTick();
    }
}
SPD::~SPD() {
    assert(tiles_data != nullptr);
    delete[] tiles_data;
    assert(tiles_ready != nullptr);
    delete[] tiles_ready;
    assert(tiles_size != nullptr);
    delete[] tiles_size;
    assert(read_port_busy_until != nullptr);
    delete[] read_port_busy_until;
    assert(write_port_busy_until != nullptr);
    delete[] write_port_busy_until;
}

///////////////
//
// RF
//
///////////////
RF::RF(unsigned int _num_regs) : num_regs(_num_regs) {
    data = new uint32_t[num_regs];
    memset(data, 0, num_regs * sizeof(uint32_t));
}
RF::~RF() {
    assert(data != nullptr);
    delete[] data;
}
} // namespace gem5