#ifndef __MEM_MAA_SPD_HH__
#define __MEM_MAA_SPD_HH__

#include <cassert>
#include <cstdint>
#include <cstring>

namespace gem5 {

class TILE {
protected:
    uint32_t *data;
    unsigned int num_tile_elements;
    uint16_t ready;
    uint16_t size;

public:
    uint32_t getData(int element_id) {
        assert((0 <= element_id) && (element_id < num_tile_elements));
        return data[element_id];
    }
    uint8_t *getDataPtr(int element_id) {
        assert((0 <= element_id) && (element_id < num_tile_elements));
        return (uint8_t *)(&data[element_id]);
    }
    void setData(int element_id, uint32_t _data) {
        assert((0 <= element_id) && (element_id < num_tile_elements));
        this->data[element_id] = _data;
    }

    uint16_t getReady() { return ready; }
    void setReady() { this->ready = 1; }
    void unsetReady() { this->ready = 0; }

    uint16_t getSize() { return size; }
    void setSize(uint16_t size) { this->size = size; }

public:
    TILE() { data = nullptr; }
    ~TILE() {
        if (data != nullptr)
            delete[] data;
    }
    void allocate(unsigned int _num_tile_elements) {
        num_tile_elements = _num_tile_elements;
        ready = 1;
        size = 0;
        data = new uint32_t[num_tile_elements];
        memset(data, 0, num_tile_elements * sizeof(uint32_t));
    }
};

class SPD {
protected:
    TILE *tiles;
    unsigned int num_tiles;
    unsigned int num_tile_elements;

public:
    uint32_t getData(int tile_id, int element_id) {
        assert((0 <= tile_id) && (tile_id < num_tiles));
        return tiles[tile_id].getData(element_id);
    }
    uint8_t *getDataPtr(int tile_id, int element_id) {
        assert((0 <= tile_id) && (tile_id < num_tiles));
        return tiles[tile_id].getDataPtr(element_id);
    }
    void setData(int tile_id, int element_id, uint32_t _data) {
        assert((0 <= tile_id) && (tile_id < num_tiles));
        tiles[tile_id].setData(element_id, _data);
    }
    uint16_t getReady(int tile_id) {
        assert((0 <= tile_id) && (tile_id < num_tiles));
        return tiles[tile_id].getReady();
    }
    void setReady(int tile_id) {
        assert((0 <= tile_id) && (tile_id < num_tiles));
        tiles[tile_id].setReady();
    }
    void unsetReady(int tile_id) {
        assert((0 <= tile_id) && (tile_id < num_tiles));
        tiles[tile_id].unsetReady();
    }
    uint16_t getSize(int tile_id) {
        assert((0 <= tile_id) && (tile_id < num_tiles));
        return tiles[tile_id].getSize();
    }
    void setSize(int tile_id, uint16_t size) {
        assert((0 <= tile_id) && (tile_id < num_tiles));
        tiles[tile_id].setSize(size);
    }

public:
    SPD(unsigned int _num_tiles, unsigned int _num_tile_elements)
        : num_tiles(_num_tiles),
          num_tile_elements(_num_tile_elements) {
        tiles = new TILE[num_tiles];
        for (int i = 0; i < num_tiles; i++) {
            tiles[i].allocate(num_tile_elements);
        }
    }
    ~SPD() {
        assert(tiles != nullptr);
        delete[] tiles;
    }
};

class RF {
protected:
    uint32_t *data;
    unsigned int num_regs;

public:
    uint32_t getData(int reg_id) {
        assert((0 <= reg_id) && (reg_id < num_regs));
        return data[reg_id];
    }
    uint8_t *getDataPtr(int reg_id) {
        assert((0 <= reg_id) && (reg_id < num_regs));
        return (uint8_t *)(&data[reg_id]);
    }
    void setData(int reg_id, uint32_t _data) {
        assert((0 <= reg_id) && (reg_id < num_regs));
        this->data[reg_id] = _data;
    }

public:
    RF(unsigned int _num_regs) : num_regs(_num_regs) {
        data = new uint32_t[num_regs];
        memset(data, 0, num_regs * sizeof(uint32_t));
    }
    ~RF() {
        assert(data != nullptr);
        delete[] data;
    }
};
} // namespace gem5
#endif // __MEM_MAA_SPD_HH__