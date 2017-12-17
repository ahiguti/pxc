
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <memory>
#include <utility>
#include <chrono>

#include "d3axim.hpp"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4091)
#include "FTD3XX.h"
#pragma warning(pop)
#pragma comment(lib, "FTD3XX.lib")
#else
#include "ftd3xx.h"
#endif

#define DBG(x)

using namespace std;

static int
read_all(FT_HANDLE hnd, UCHAR pipeid, uint8_t *buf, ULONG buflen, ULONG *count)
{
    FT_STATUS r = FT_OK;
    *count = 0;
    while (buflen > 0) {
        ULONG rl = 0;
        r = FT_ReadPipe(hnd, pipeid, buf, buflen, &rl, NULL);
        if (r != FT_OK) {
            return r;
        }
        *count += rl;
        buflen -= rl;
        buf += rl;
    }
    return r;
}

struct ftd3_overlapped {
    unsigned long len;
    OVERLAPPED ov;
    FT_STATUS st;
};

struct ftd3_handle {
    ftd3_handle(unsigned index, int use_overlapped = false)
        : use_overlapped(use_overlapped)
    {
        FT_Create((PVOID)(intptr_t)index, FT_OPEN_BY_INDEX, &handle);
        if (!handle) {
            return;        
        }
        ovs.resize(use_overlapped ? (use_overlapped & 255): 1);
        for (auto& ov : ovs) {
            ov.len = 0;
            ov.st = FT_OK;
            FT_InitializeOverlapped(handle, &ov.ov);
        }
        bool suc = true;
        FT_60XCONFIGURATION cnf = {};
        if (FT_GetChipConfiguration(handle, &cnf) != FT_OK) {
            DBG(fprintf(stdout, "FT_GetChipConfiguration failed\n"));
            suc = false;
        }
        if (cnf.ChannelConfig == CONFIGURATION_CHANNEL_CONFIG_1_INPIPE) {
            DBG(fprintf(stdout, "ChannelConfig is INPIPE\n"));
            suc = false;
        }
        if (cnf.ChannelConfig == CONFIGURATION_CHANNEL_CONFIG_1_OUTPIPE) {
            DBG(fprintf(stdout, "ChannelConfig is OUTPIPE\n"));
            suc = false;
        }
        DBG(fprintf(stdout, "ChannelConfig=%d\n", cnf.ChannelConfig));
        if (!suc) {
            FT_Close(handle);
            handle = 0;
        }
    }
    ~ftd3_handle()
    {
        for (auto& ov : ovs) {
            FT_ReleaseOverlapped(handle, &ov.ov);
        }
        FT_Close(handle);
    }
    FT_STATUS write_pipe(uint8_t *data, ULONG& len)
    {
        if (!use_overlapped) {
            return FT_WritePipe(handle, 0x02, data, len, &len, NULL);
        }
        FT_STATUS r = FT_OK;
        ULONG dlen = static_cast<ULONG>((len + ovs.size() - 1) / ovs.size());
        ULONG offset = 0;
        ULONG len_r = 0;
        for (auto& ov : ovs) {
            if (ov.len != 0) {
                ULONG wlen = 0;
                if ((use_overlapped & 256) != 0) {
#ifdef _MSC_VER
                    // busy polling
                    do {
                        ov.st = FT_GetOverlappedResult(handle, &ov.ov, &wlen,
                            FALSE);
                    } while (ov.st == FT_IO_PENDING);
#endif
                } else {
                    ov.st = FT_GetOverlappedResult(handle, &ov.ov, &wlen, 1);
                }
                if (ov.st != FT_OK) {
                    DBG(fprintf(stdout, "FT_GetOverlappedResult: %d\n",
                        ov.st));
                } else if (wlen != ov.len) {
                    DBG(fprintf(stdout,
                        "FT_GetOverlappedResult: len %lu of %lu\n",
                        (unsigned long)wlen, (unsigned long)ov.len));
                    ov.st = FT_FAILED_TO_WRITE_DEVICE;
                }
                r = ov.st;
                len_r += wlen;
                ov.len = 0;
            }
            ov.len = min(dlen, len);
            ov.st = FT_OK;
            len -= ov.len;
            if (ov.len != 0) {
                ULONG wlen = 0;
                ov.st = FT_WritePipe(handle, 0x02, data + offset, ov.len,
                    &wlen, &ov.ov);
                offset += ov.len;
                if (ov.st != FT_IO_PENDING) {
                    DBG(fprintf(stdout, "FT_WritePipe: %d\n", ov.st));
                    r = ov.st;
                }
            }
        }
        len = len_r;
        return r;
    }
    FT_STATUS read_pipe(uint8_t *data, ULONG& len)
    {
        return FT_ReadPipe(handle, 0x82, data, len, &len, NULL);
    }
    FT_HANDLE get() const
    {
        return handle;
    }
private:
    int use_overlapped;
    FT_HANDLE handle;
    ftd3_handle(const ftd3_handle&) = delete;
    ftd3_handle& operator =(const ftd3_handle&) = delete;
    std::vector<ftd3_overlapped> ovs;
};

struct ftd3_aximaster::impl_type {
    impl_type(unsigned index)
        : handle(index) { }
    void clear_tr();
    void prepare_write(uint32_t address, uint8_t const *start,
        uint8_t const *finish);
    void prepare_read(uint32_t address, uint32_t size);
    bool exec_tr();
    int test_empty();
    ftd3_handle handle;
    std::vector<uint8_t> buf;
    size_t tr_resp_size = 0;
};

ftd3_aximaster::ftd3_aximaster(unsigned index)
    : impl(new impl_type(index))
{
}

ftd3_aximaster::~ftd3_aximaster()
{
}

ftd3_aximaster::operator bool() const
{
    return impl->handle.get() != 0;
}

void
ftd3_aximaster::impl_type::clear_tr()
{
    buf.clear();
    tr_resp_size = 0;
}

void
ftd3_aximaster::impl_type::prepare_write(uint32_t address,
    uint8_t const *start, uint8_t const *finish)
{
    size_t count = (finish - start) >> 2u;
    if (count == 0u) {
        return;
    }
    uint32_t hdr[2];
    hdr[0] = 0x80000000 | (address >> 2u);
    hdr[1] = static_cast<uint32_t>(count - 1u);
    uint8_t const *const p = reinterpret_cast<uint8_t const *>(hdr);
    buf.insert(buf.end(), p, p + 8);
    buf.insert(buf.end(), start, start + (count << 2u));
#if 0
    for (size_t i = 0; i < 8; ++i) {
        DBG(fprintf(stdout, "buf %02x\r\n", (unsigned)buf[i]));
    }
    fprintf(stdout, "count=%x\r\n", (unsigned)count);
#endif
}

void
ftd3_aximaster::impl_type::prepare_read(uint32_t address, uint32_t size)
{
    size_t count = size >> 2u;
    if (count == 0u) {
        return;
    }
    uint32_t hdr[2];
    hdr[0] = (address >> 2u);
    hdr[1] = static_cast<uint32_t>(count - 1u);
    uint8_t const *const p = reinterpret_cast<uint8_t const *>(hdr);
    buf.insert(buf.end(), p, p + 8);
    tr_resp_size += (count << 2u);
#if 0
    for (size_t i = 0; i < buf.size(); ++i) {
        DBG(fprintf(stdout, "buf %02x\r\n", (unsigned)buf[i]));
    }
#endif
}

bool
ftd3_aximaster::impl_type::exec_tr()
{
    bool suc = true;
    if (!buf.empty()) {
        ULONG len = static_cast<ULONG>(buf.size());
        FT_STATUS r = handle.write_pipe(buf.data(), len);
        if (r != FT_OK) {
            DBG(fprintf(stdout, "Failed to write %d\r\n", (int)r));
            suc = false;
        }
        DBG(fprintf(stdout, "Wrote %u bytes\r\n", len));
    }
    if (tr_resp_size != 0u) {
        buf.resize(tr_resp_size);
        ULONG count = 0;
        FT_STATUS r = read_all(handle.get(), 0x82, buf.data(),
			static_cast<ULONG>(buf.size()), &count);
        if (r != FT_OK) {
            DBG(fprintf(stdout, "Failed to read\r\n"));
            suc = false;
        }
        DBG(fprintf(stdout, "Read %d bytes\r\n", count));
    }
    return suc;
}

int
ftd3_aximaster::impl_type::test_empty()
{
    int i = 0;
    for (i = 0; i < 4096; ++i) {
        ULONG count = 0;
        buf.resize(1);
        FT_STATUS r = read_all(handle.get(), 0x82, buf.data(),
			static_cast<ULONG>(buf.size()), &count);
        if (r != FT_OK) {
            break;
        }
        fprintf(stdout, "test_empty: read %d\n", i);
    }
    return (int)i;
}

bool
ftd3_aximaster::axi_write(uint32_t address, uint8_t const *start,
    uint8_t const *finish)
{
    impl->clear_tr();
    impl->prepare_write(address, start, finish);
    return impl->exec_tr();
}

std::pair<uint8_t const *, uint8_t const *>
ftd3_aximaster::axi_read(uint32_t address, uint32_t size)
{
    impl->clear_tr();
    impl->prepare_read(address, size);
    if (impl->exec_tr()) {
        return std::make_pair(impl->buf.data(),
            impl->buf.data() + impl->buf.size());
    }
    return { };
}

int
ftd3_aximaster::test_empty()
{
  return impl->test_empty();
}

