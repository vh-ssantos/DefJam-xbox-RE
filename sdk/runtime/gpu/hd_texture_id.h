#pragma once
// (DefJam) Shared, address-independent HD identity. Compatible with the MIT
// DefJamFFNY-recomp XRTEX01 identity (vyanhursky); no game data lives here.
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <string>

namespace defjam::hd {
class Sha256 {
    std::array<uint32_t, 8> state_{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                                   0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    std::array<uint8_t,64> block_{};
    uint64_t bytes_ = 0;
    size_t used_ = 0;
    void Block() {
        static constexpr uint32_t k[]{
            0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
            0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
            0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
            0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
            0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
            0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
            0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
            0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
        uint32_t w[64];
        for (unsigned i=0;i<16;i++) w[i]=uint32_t(block_[i*4])<<24|uint32_t(block_[i*4+1])<<16|
            uint32_t(block_[i*4+2])<<8|block_[i*4+3];
        for (unsigned i=16;i<64;i++) {
            auto a=w[i-15],b=w[i-2];
            w[i]=w[i-16]+(std::rotr(a,7)^std::rotr(a,18)^(a>>3))+w[i-7]+
                (std::rotr(b,17)^std::rotr(b,19)^(b>>10));
        }
        auto [a,b,c,d,e,f,g,h]=state_;
        for (unsigned i=0;i<64;i++) {
            uint32_t t=h+(std::rotr(e,6)^std::rotr(e,11)^std::rotr(e,25))+((e&f)^(~e&g))+k[i]+w[i];
            uint32_t u=(std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22))+((a&b)^(a&c)^(b&c));
            h=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+u;
        }
        state_[0]+=a;state_[1]+=b;state_[2]+=c;state_[3]+=d;
        state_[4]+=e;state_[5]+=f;state_[6]+=g;state_[7]+=h;
    }
public:
    void Update(std::span<const uint8_t> data) {
        bytes_+=data.size();
        while (!data.empty()) {
            size_t n=std::min(data.size(),64-used_);
            std::copy_n(data.begin(),n,block_.begin()+used_);
            used_+=n;data=data.subspan(n);
            if (used_==64) { Block();used_=0; }
        }
    }
    std::string Finish() {
        uint64_t bits=bytes_*8;
        block_[used_++]=0x80;
        if (used_>56) { std::fill(block_.begin()+used_,block_.end(),0);Block();used_=0; }
        std::fill(block_.begin()+used_,block_.begin()+56,0);
        for (unsigned i=0;i<8;i++) block_[63-i]=uint8_t(bits>>(8*i));
        Block();
        constexpr char hex[]="0123456789abcdef";
        std::string out(64,'0');
        for (unsigned i=0;i<32;i++) {
            uint8_t v=uint8_t(state_[i/4]>>(24-8*(i%4)));
            out[i*2]=hex[v>>4];out[i*2+1]=hex[v&15];
        }
        return out;
    }
};

// Base-level bytes only: DXT blocks or original Morton texels. Palette is BGRA,
// not a pointer; linear rows are tight. row_texels defaults to visible width.
inline std::string TextureId(uint32_t format,uint32_t width,uint32_t height,
    std::span<const uint8_t> pixels,std::span<const uint8_t> palette={},uint32_t row_texels=0) {
    Sha256 sha;
    constexpr uint8_t magic[]{'X','R','T','E','X','0','1',0};
    sha.Update(magic);
    const uint32_t values[]{format,width,height,uint32_t(palette.size()/4),row_texels?row_texels:width};
    uint8_t header[20];
    for (unsigned i=0;i<5;i++) for (unsigned j=0;j<4;j++) header[4*i+j]=uint8_t(values[i]>>(8*j));
    sha.Update(header);sha.Update(pixels);sha.Update(palette);
    return sha.Finish();
}
} // namespace defjam::hd
