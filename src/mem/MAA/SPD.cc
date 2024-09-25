#include "mem/MAA/SPD.hh"
#include "base/trace.hh"
#include "mem/MAA/MAA.hh"
#include "base/types.hh"
#include "debug/SPD.hh"
#include "sim/cur_tick.hh"
#include <cassert>

#ifndef TRACING_ON
#define TRACING_ON 1
#endif

namespace gem5 {

///////////////
//
// TILE
//
///////////////
uint8_t *TILE::getDataPtr(int element_id) {
    assert((0 <= element_id) && (element_id < num_tile_elements));
    return (uint8_t *)(&data[element_id]);
}
uint16_t TILE::getReady() {
    return ready;
}
void TILE::setReady() {
    this->ready = 1;
}
void TILE::unsetReady() {
    this->ready = 0;
}

uint16_t TILE::getSize() {
    return size;
}
void TILE::setSize(uint16_t size) {
    this->size = size;
}
void TILE::allocate(unsigned int _num_tile_elements) {
    num_tile_elements = _num_tile_elements;
    ready = 1;
    size = 0;
    data = new uint32_t[num_tile_elements];
    memset(data, 0, num_tile_elements * sizeof(uint32_t));
}
TILE::TILE() { data = nullptr; }
TILE::~TILE() {
    if (data != nullptr)
        delete[] data;
}

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
uint8_t *SPD::getDataPtr(int tile_id, int element_id) {
    assert((0 <= tile_id) && (tile_id < num_tiles));
    return tiles[tile_id].getDataPtr(element_id);
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
    assert((0 <= tile_id) && (tile_id < num_tiles));
    return tiles[tile_id].getReady();
}
void SPD::setReady(int tile_id) {
    assert((0 <= tile_id) && (tile_id < num_tiles));
    tiles[tile_id].setReady();
}
void SPD::unsetReady(int tile_id) {
    assert((0 <= tile_id) && (tile_id < num_tiles));
    tiles[tile_id].unsetReady();
}
uint16_t SPD::getSize(int tile_id) {
    assert((0 <= tile_id) && (tile_id < num_tiles));
    panic_if(getReady(tile_id) == 0,
             "Trying to get size of an uninitialized tile[%d]!\n",
             tile_id);
    return tiles[tile_id].getSize();
}
void SPD::setSize(int tile_id, uint16_t size) {
    assert((0 <= tile_id) && (tile_id < num_tiles));
    tiles[tile_id].setSize(size);
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
    tiles = new TILE[num_tiles];
    for (int i = 0; i < num_tiles; i++) {
        tiles[i].allocate(num_tile_elements);
    }
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
    assert(tiles != nullptr);
    delete[] tiles;
}

///////////////
//
// RF
//
///////////////
uint8_t *RF::getDataPtr(int reg_id) {
    assert((0 <= reg_id) && (reg_id < num_regs));
    return (uint8_t *)(&data[reg_id]);
}
RF::RF(unsigned int _num_regs) : num_regs(_num_regs) {
    data = new uint32_t[num_regs];
    memset(data, 0, num_regs * sizeof(uint32_t));
}
RF::~RF() {
    assert(data != nullptr);
    delete[] data;
}
} // namespace gem5