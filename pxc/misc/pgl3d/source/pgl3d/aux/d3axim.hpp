
#ifndef D3AXIM_HPP
#define D3AXIM_HPP

#include <stdint.h>
#include <memory>

struct ftd3_aximaster {
    ftd3_aximaster(unsigned index = 0);
	~ftd3_aximaster();
    operator bool() const;
    bool axi_write(uint32_t address, uint8_t const *start,
        uint8_t const *finish);
    std::pair<uint8_t const *, uint8_t const *> axi_read(uint32_t address,
        uint32_t size);
    int test_empty();
private:
    struct impl_type;
    std::unique_ptr<impl_type> impl;
};

#endif

