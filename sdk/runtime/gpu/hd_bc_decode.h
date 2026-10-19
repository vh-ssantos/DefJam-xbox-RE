#pragma once
// (DefJam) Standalone Xbox BC1/BC2/BC3 base-level decoder used by the local HD
// builder and by decoded-content identity fallback. Output is tight BGRA8.
#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace defjam::hd {
inline bool DecodeBC(uint32_t format,uint32_t w,uint32_t h,std::span<const uint8_t> src,std::vector<uint8_t>& out) {
    unsigned block_size=format==0x0c?8:16;
    if (format!=0x0c&&format!=0x0e&&format!=0x0f) return false;
    if (!w||!h||w>4096||h>4096||src.size()!=size_t((w+3)/4)*((h+3)/4)*block_size) return false;
    out.assign(size_t(w)*h*4,0);
    auto u16=[](const uint8_t* p){return uint16_t(p[0]|uint16_t(p[1])<<8);};
    auto colour=[](uint16_t c,uint8_t* p){
        uint32_t b=c&31,g=(c>>5)&63,r=c>>11;
        p[0]=uint8_t((b<<3)|(b>>2));p[1]=uint8_t((g<<2)|(g>>4));
        p[2]=uint8_t((r<<3)|(r>>2));p[3]=255;
    };
    size_t at=0;
    for (uint32_t by=0;by<h;by+=4) for (uint32_t bx=0;bx<w;bx+=4,at+=block_size) {
        const uint8_t* block=src.data()+at;
        const uint8_t* c=block+(block_size==16?8:0);
        uint16_t a=u16(c),b=u16(c+2);
        uint8_t colors[4][4]{};colour(a,colors[0]);colour(b,colors[1]);
        if (a>b||format!=0x0c) {
            for (unsigned k=0;k<3;k++) {
                colors[2][k]=uint8_t((2*colors[0][k]+colors[1][k])/3);
                colors[3][k]=uint8_t((colors[0][k]+2*colors[1][k])/3);
            }
            colors[2][3]=colors[3][3]=255;
        } else {
            for (unsigned k=0;k<3;k++) colors[2][k]=uint8_t((colors[0][k]+colors[1][k])/2);
            colors[2][3]=255;
        }
        uint32_t codes=uint32_t(c[4])|uint32_t(c[5])<<8|uint32_t(c[6])<<16|uint32_t(c[7])<<24;
        uint8_t alpha[8]{};uint64_t acodes=0;
        if (format==0x0f) {
            alpha[0]=block[0];alpha[1]=block[1];
            if (alpha[0]>alpha[1]) for (unsigned j=2;j<8;j++) alpha[j]=uint8_t(((8-j)*alpha[0]+(j-1)*alpha[1])/7);
            else {
                for (unsigned j=2;j<6;j++) alpha[j]=uint8_t(((6-j)*alpha[0]+(j-1)*alpha[1])/5);
                alpha[6]=0;alpha[7]=255;
            }
            for (unsigned j=0;j<6;j++) acodes|=uint64_t(block[j+2])<<(j*8);
        }
        for (unsigned p=0;p<16;p++) {
            uint32_t x=bx+p%4,y=by+p/4;
            if (x>=w||y>=h) continue;
            uint8_t* target=out.data()+(size_t(y)*w+x)*4;
            std::copy_n(colors[(codes>>(p*2))&3],4,target);
            if (format==0x0e) target[3]=uint8_t(((block[p/2]>>((p%2)*4))&15)*17);
            if (format==0x0f) target[3]=alpha[(acodes>>(p*3))&7];
        }
    }
    return true;
}
} // namespace defjam::hd
