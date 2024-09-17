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
    uint32_t getData(int element_id);
    uint8_t *getDataPtr(int element_id);
    void setData(int element_id, uint32_t _data);

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

public:
    uint32_t getData(int tile_id, int element_id);
    uint8_t *getDataPtr(int tile_id, int element_id);
    void setData(int tile_id, int element_id, uint32_t _data);
    uint16_t getReady(int tile_id);
    void setReady(int tile_id);
    void unsetReady(int tile_id);
    uint16_t getSize(int tile_id);
    void setSize(int tile_id, uint16_t size);

public:
    SPD(unsigned int _num_tiles, unsigned int _num_tile_elements);
    ~SPD();
};

class RF {
protected:
    uint32_t *data;
    unsigned int num_regs;

public:
    uint32_t getData(int reg_id);
    uint8_t *getDataPtr(int reg_id);
    void setData(int reg_id, uint32_t _data);

public:
    RF(unsigned int _num_regs);
    ~RF();
};
} // namespace gem5
#endif // __MEM_MAA_SPD_HH__