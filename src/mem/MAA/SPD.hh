#ifndef __MEM_MAA_SPD_HH__
#define __MEM_MAA_SPD_HH__

#include <cassert>
#include <cstdint>
#include <cstring>
#include "base/types.hh"

namespace gem5 {
class MAA;

class TILE {
protected:
    uint32_t *data;
    unsigned int num_tile_elements;
    uint16_t ready;
    uint16_t size;

public:
    template <typename T>
    T getData(int element_id) {
        assert((0 <= element_id) && (element_id < num_tile_elements));
        return ((T *)data)[element_id];
    }
    uint8_t *getDataPtr(int element_id);
    template <typename T>
    void setData(int element_id, T _data) {
        assert((0 <= element_id) && (element_id < num_tile_elements));
        ((T *)this->data)[element_id] = _data;
    }

    uint16_t getReady();
    void setReady();
    void unsetReady();

    uint16_t getSize();
    void setSize(uint16_t size);

public:
    TILE();
    ~TILE();
    void allocate(unsigned int _num_tile_elements);
};

class SPD {
protected:
    TILE *tiles;
    unsigned int num_tiles;
    unsigned int num_tile_elements;
    Tick *read_port_busy_until;
    Tick *write_port_busy_until;
    const Cycles read_latency, write_latency;
    const int num_read_ports, num_write_ports;
    MAA *maa;

public:
    template <typename T>
    T getData(int tile_id, int element_id) {
        assert((0 <= tile_id) && (tile_id < num_tiles));
        return tiles[tile_id].getData<T>(element_id);
    }
    Cycles getDataLatency(int num_accesses);
    uint8_t *getDataPtr(int tile_id, int element_id);
    template <typename T>
    void setData(int tile_id, int element_id, T _data) {
        assert((0 <= tile_id) && (tile_id < num_tiles));
        tiles[tile_id].setData<T>(element_id, _data);
    }
    Cycles setDataLatency(int num_accesses);
    uint16_t getReady(int tile_id);
    void setReady(int tile_id);
    void unsetReady(int tile_id);
    uint16_t getSize(int tile_id);
    void setSize(int tile_id, uint16_t size);

public:
    SPD(MAA *_maa,
        unsigned int _num_tiles,
        unsigned int _num_tile_elements,
        Cycles _read_latency,
        Cycles _write_latency,
        int _num_read_ports,
        int _num_write_ports);

    ~SPD();
};

class RF {
protected:
    uint32_t *data;
    unsigned int num_regs;

public:
    template <typename T>
    T getData(int reg_id) {
        assert((0 <= reg_id) && (reg_id < num_regs));
        return ((T *)data)[reg_id];
    }
    uint8_t *getDataPtr(int reg_id);
    template <typename T>
    void setData(int reg_id, T _data) {
        assert((0 <= reg_id) && (reg_id < num_regs));
        ((T *)this->data)[reg_id] = _data;
    }

public:
    RF(unsigned int _num_regs);
    ~RF();
};
} // namespace gem5
#endif // __MEM_MAA_SPD_HH__