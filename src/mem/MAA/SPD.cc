#include "mem/MAA/SPD.hh"
#include "mem/MAA/MAA.hh"
#include "debug/MAA.hh"
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
uint32_t TILE::getData(int element_id) {
    assert((0 <= element_id) && (element_id < num_tile_elements));
    return data[element_id];
}
uint8_t *TILE::getDataPtr(int element_id) {
    assert((0 <= element_id) && (element_id < num_tile_elements));
    return (uint8_t *)(&data[element_id]);
}
void TILE::setData(int element_id, uint32_t _data) {
    assert((0 <= element_id) && (element_id < num_tile_elements));
    this->data[element_id] = _data;
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
uint32_t SPD::getData(int tile_id, int element_id) {
    assert((0 <= tile_id) && (tile_id < num_tiles));
    return tiles[tile_id].getData(element_id);
}
uint8_t *SPD::getDataPtr(int tile_id, int element_id) {
    assert((0 <= tile_id) && (tile_id < num_tiles));
    return tiles[tile_id].getDataPtr(element_id);
}
void SPD::setData(int tile_id, int element_id, uint32_t _data) {
    assert((0 <= tile_id) && (tile_id < num_tiles));
    tiles[tile_id].setData(element_id, _data);
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
    return tiles[tile_id].getSize();
}
void SPD::setSize(int tile_id, uint16_t size) {
    assert((0 <= tile_id) && (tile_id < num_tiles));
    tiles[tile_id].setSize(size);
}
SPD::SPD(unsigned int _num_tiles, unsigned int _num_tile_elements)
    : num_tiles(_num_tiles),
      num_tile_elements(_num_tile_elements) {
    tiles = new TILE[num_tiles];
    for (int i = 0; i < num_tiles; i++) {
        tiles[i].allocate(num_tile_elements);
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
uint32_t RF::getData(int reg_id) {
    assert((0 <= reg_id) && (reg_id < num_regs));
    return data[reg_id];
}
uint8_t *RF::getDataPtr(int reg_id) {
    assert((0 <= reg_id) && (reg_id < num_regs));
    return (uint8_t *)(&data[reg_id]);
}
void RF::setData(int reg_id, uint32_t _data) {
    assert((0 <= reg_id) && (reg_id < num_regs));
    this->data[reg_id] = _data;
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