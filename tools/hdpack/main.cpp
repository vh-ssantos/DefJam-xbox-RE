// Local, own-disc HD builder. Archive research/identity adapted from the MIT
// vyanhursky/DefJamFFNY-recomp scripts/hd_assets.py. No game bytes are embedded.
#include "../../sdk/runtime/gpu/hd_texture_id.h"
#include "../../sdk/runtime/gpu/hd_bc_decode.h"
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace fs=std::filesystem;
using Bytes=std::vector<uint8_t>;
using View=std::span<const uint8_t>;
using Microsoft::WRL::ComPtr;
constexpr size_t MaxBlob=128ull*1024*1024;
static void Check(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
static View Slice(View b,size_t at,size_t n) {
    Check(at<=b.size()&&n<=b.size()-at,"Truncated source data");return b.subspan(at,n);
}
static uint32_t U32(View b,size_t at) {
    auto p=Slice(b,at,4);return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;
}
static uint16_t U16(View b,size_t at) { auto p=Slice(b,at,2);return uint16_t(p[0]|uint16_t(p[1])<<8); }
static uint32_t BE(View b,size_t at,unsigned n=4) { uint32_t v=0;for (uint8_t x:Slice(b,at,n)) v=(v<<8)|x;return v; }
static bool Magic(View b,const char* s) { return b.size()>=4&&std::equal(b.begin(),b.begin()+4,s); }
static std::string Lower(std::string s) { for (char& c:s) c=char(std::tolower(uint8_t(c)));return s; }
static Bytes ReadFile(const fs::path& path) {
    auto size=fs::file_size(path);Check(size<=MaxBlob,"File exceeds archive budget");
    Bytes b(size);std::ifstream in(path,std::ios::binary);in.read(reinterpret_cast<char*>(b.data()),b.size());
    Check(bool(in),"Cannot read source file");return b;
}
static std::string HashFile(const fs::path& path) {
    std::ifstream in(path,std::ios::binary);Check(bool(in),"Cannot hash file");
    defjam::hd::Sha256 sha;uint8_t buffer[65536];
    while (in) { in.read(reinterpret_cast<char*>(buffer),sizeof(buffer));sha.Update(View(buffer,size_t(in.gcount()))); }
    Check(in.eof(),"File checksum read failed");return sha.Finish();
}
static void AtomicReplace(const fs::path& pending,const fs::path& final) {
    Check(MoveFileExW(pending.c_str(),final.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,
          "Cannot publish HD file");
}
static void Hr(HRESULT value,const char* message) { Check(SUCCEEDED(value),message); }
struct Com {
    Com() { Hr(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"Cannot initialize image service"); }
    ~Com() { CoUninitialize(); }
};
static ComPtr<IWICImagingFactory> Factory() {
    ComPtr<IWICImagingFactory> f;
    Hr(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&f)),"Cannot create PNG service");return f;
}
static void Png(IWICImagingFactory* factory,const fs::path& path,uint32_t w,uint32_t h,View bgra) {
    Check(bgra.size()==size_t(w)*h*4,"Invalid image output size");
    ComPtr<IWICStream> stream;Hr(factory->CreateStream(&stream),"Cannot create PNG stream");
    Hr(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE),"Cannot create PNG file");
    ComPtr<IWICBitmapEncoder> encoder;Hr(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder),"Cannot create PNG encoder");
    Hr(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache),"Cannot initialize PNG encoder");
    ComPtr<IWICBitmapFrameEncode> frame;Hr(encoder->CreateNewFrame(&frame,nullptr),"Cannot create PNG frame");
    Hr(frame->Initialize(nullptr),"Cannot initialize PNG frame");Hr(frame->SetSize(w,h),"Cannot set PNG dimensions");
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;
    Hr(frame->SetPixelFormat(&format),"Cannot set PNG channels");Check(IsEqualGUID(format,GUID_WICPixelFormat32bppBGRA),"PNG BGRA unavailable");
    Hr(frame->WritePixels(h,w*4,UINT(bgra.size()),const_cast<BYTE*>(bgra.data())),"Cannot encode PNG pixels");
    Hr(frame->Commit(),"Cannot commit PNG frame");Hr(encoder->Commit(),"Cannot commit PNG file");
}

// Xbox XDVDFS listing; mirrors sdk/runtime/kernel/disc.cpp, but never mounts a
// guest drive and reads one archive at a time rather than extracting the disc.
struct SourceFile { std::string name;fs::path path;uint64_t offset=0;uint32_t size=0; };
class Sources {
    std::ifstream iso_;uint64_t iso_size_=0,base_=0;std::set<uint64_t> visited_;
    Bytes At(uint64_t at,size_t size) {
        Check(at<=iso_size_&&size<=iso_size_-at&&size<=MaxBlob,"Invalid ISO extent");
        iso_.clear();iso_.seekg(at);Bytes b(size);iso_.read(reinterpret_cast<char*>(b.data()),size);
        Check(bool(iso_),"Short ISO read");return b;
    }
    void Dir(uint32_t sector,uint32_t size,std::string prefix,unsigned depth) {
        Check(depth<=32&&size<=16*1024*1024,"ISO directory limit");
        if (!size) return;
        uint64_t address=base_+uint64_t(sector)*2048;
        Check(visited_.insert(address).second,"Cyclic ISO directory");
        auto b=At(address,size);std::vector<size_t> todo{0};std::set<size_t> seen;
        while (!todo.empty()) {
            size_t p=todo.back();todo.pop_back();
            Check(p%4==0&&p+14<=b.size(),"ISO directory entry outside table");
            Check(seen.insert(p).second,"Cyclic ISO directory tree");
            auto left=U16(b,p),right=U16(b,p+2);if (left==0xffff) continue;
            if (left) todo.push_back(size_t(left)*4);if (right) todo.push_back(size_t(right)*4);
            auto name_bytes=Slice(b,p+14,b[p+13]);
            std::string name(reinterpret_cast<const char*>(name_bytes.data()),name_bytes.size());
            Check(!name.empty()&&name!="."&&name!=".."&&name.find_first_of("/\\\0",0,3)==std::string::npos,"Invalid ISO filename");
            uint32_t child_sector=U32(b,p+4),child_size=U32(b,p+8);
            if (b[p+12]&0x10) Dir(child_sector,child_size,prefix+name+"/",depth+1);
            else files.push_back({prefix+name,{},base_+uint64_t(child_sector)*2048,child_size});
        }
    }
public:
    std::vector<SourceFile> files;
    explicit Sources(const fs::path& input) {
        if (fs::is_directory(input)) {
            for (const auto& e:fs::recursive_directory_iterator(input)) if (e.is_regular_file()) {
                auto size=e.file_size();if(size<=UINT32_MAX)files.push_back({fs::relative(e.path(),input).generic_string(),e.path(),0,uint32_t(size)});
            }
        } else {
            iso_size_=fs::file_size(input);iso_.open(input,std::ios::binary);Check(bool(iso_),"Cannot open ISO");
            bool found=false;const std::string magic="MICROSOFT*XBOX*MEDIA";
            for (uint64_t base:{0ull,0x18300000ull}) {
                if (base+65536+2048>iso_size_)continue;
                auto b=At(base+65536,2048);
                if (std::equal(magic.begin(),magic.end(),b.begin())&&std::equal(magic.begin(),magic.end(),b.begin()+0x7ec)) {
                    base_=base;Dir(U32(b,20),U32(b,24),"",0);found=true;break;
                }
            }
            Check(found,"No Xbox filesystem found in ISO");
        }
        std::sort(files.begin(),files.end(),[](const auto& a,const auto& b){return Lower(a.name)<Lower(b.name);});
    }
    Bytes Load(const SourceFile& file) { return file.path.empty()?At(file.offset,file.size):ReadFile(file.path); }
};

static Bytes Refpack(View b) {
    uint16_t signature=uint16_t(BE(b,0,2));unsigned step=signature&0x8000?4:3;
    size_t p=2+(signature&0x0100?step:0);uint32_t expected=BE(b,p,step);p+=step;
    Check(expected<=MaxBlob,"RefPack output budget exceeded");Bytes out;out.reserve(expected);bool ended=false;
    while (p<b.size()) {
        uint8_t a=Slice(b,p++,1)[0];size_t run,length=0,dist=0;
        if (a<0x80) { uint8_t c=Slice(b,p++,1)[0];run=a&3;length=((a&0x1c)>>2)+3;dist=((a&0x60)<<3)+c+1; }
        else if (a<0xc0) { auto c=Slice(b,p,2);p+=2;run=(c[0]>>6)&3;length=(a&0x3f)+4;dist=((c[0]&0x3f)<<8)+c[1]+1; }
        else if (a<0xe0) { auto c=Slice(b,p,3);p+=3;run=a&3;length=((a&0xc)<<6)+c[2]+5;dist=((a&0x10)<<12)+(c[0]<<8)+c[1]+1; }
        else run=a<0xfc?((a&0x1f)<<2)+4:a&3;
        Check(out.size()+run+length<=expected,"RefPack expansion exceeds advertised size");
        auto literal=Slice(b,p,run);p+=run;out.insert(out.end(),literal.begin(),literal.end());
        Check(!length||(dist&&dist<=out.size()),"Invalid RefPack backreference");
        for (size_t n=0;n<length;n++)out.push_back(out[out.size()-dist]);
        if (a>=0xfc) { ended=true;break; }
    }
    Check(ended&&out.size()==expected,"Incomplete RefPack stream");return out;
}
using Leaf=std::function<void(View,const std::string&)>;
static void Walk(View b,const std::string& label,const Leaf& leaf,unsigned depth=0) {
    Check(depth<=12&&b.size()<=MaxBlob,"Archive nesting or size budget exceeded");
    if (b.size()>=2&&(BE(b,0,2)&0x3eff)==0x10fb) { auto decoded=Refpack(b);Walk(decoded,label,leaf,depth+1); }
    else if (Magic(b,"BIGF")||Magic(b,"BIG4")) {
        Check(b.size()>=16,"Short BIG header");uint32_t count=BE(b,8);Check(count<=100000,"BIG directory budget exceeded");size_t p=16;
        for (uint32_t n=0;n<count;n++) {
            uint32_t off=BE(b,p),size=BE(b,p+4);size_t start=p+8;
            p=start;while (p<b.size()&&p-start<4096&&b[p])p++;
            Check(p<b.size()&&p-start<4096,"Unterminated BIG filename");
            std::string name(reinterpret_cast<const char*>(b.data()+start),p-start);p++;
            Walk(Slice(b,off,size),label+"::"+name,leaf,depth+1);
        }
    } else leaf(b,label);
}

struct Image { uint32_t w=0,h=0,fmt=0;View pixels,palette;bool swizzled=false; };
static Image Shpx(View b,size_t offset) {
    Slice(b,offset,16);uint8_t kind=b[offset];Check(kind!=0x2a,"palette");
    Image image;image.w=U16(b,offset+4);image.h=U16(b,offset+6);uint32_t flags=U32(b,offset+12);
    Check(image.w&&image.h&&std::max(image.w,image.h)<=4096,"Invalid SHPX dimensions");
    Check(!(flags>>28),"Unsupported SHPX embedded mip layout");image.swizzled=(flags&0x2000)!=0;
    size_t data=offset+(flags&0x1000?U32(b,offset+16):16),size=0;
    Check(data>=offset+16,"Invalid SHPX indirect pointer");
    switch (kind) {
        case 0x60:case 0x61:case 0x62:
            image.fmt=kind==0x60?0x0c:kind==0x61?0x0e:0x0f;
            size=size_t((image.w+3)/4)*((image.h+3)/4)*(kind==0x60?8:16);break;
        case 0x7b:case 0x7d:
            image.fmt=kind==0x7b?0x0b:image.swizzled?0x06:0x12;
            size=size_t(image.w)*image.h*(kind==0x7b?1:4);break;
        default:throw std::runtime_error("Unsupported SHPX pixel encoding "+std::to_string(kind));
    }
    image.pixels=Slice(b,data,size);
    if (kind==0x7b) {
        size_t pos=offset;std::set<size_t> seen;
        while (seen.insert(pos).second) {
            Check(seen.size()<=64,"SHPX attachment budget exceeded");Slice(b,pos,16);
            size_t delta=uint32_t(b[pos+1])|uint32_t(b[pos+2])<<8|uint32_t(b[pos+3])<<16;
            if (!delta) break;pos+=delta;Slice(b,pos,16);
            if (b[pos]==0x2a) {
                unsigned entries=U16(b,pos+4);Check(entries&&entries<=256,"Invalid SHPX palette");
                image.palette=Slice(b,pos+16,entries*4);break;
            }
        }
        Check(!image.palette.empty(),"Missing SHPX palette");
    }
    return image;
}
static Bytes Decode(const Image& image) {
    Bytes out;
    if (image.fmt==0x0c||image.fmt==0x0e||image.fmt==0x0f) {
        Check(defjam::hd::DecodeBC(image.fmt,image.w,image.h,image.pixels,out),"Invalid BC texture");return out;
    }
    unsigned bpp=image.fmt==0x0b?1:4;out.resize(size_t(image.w)*image.h*4);
    for (unsigned y=0;y<image.h;y++) for (unsigned x=0;x<image.w;x++) {
        size_t source=size_t(y)*image.w+x;
        if (image.swizzled) {
            source=0;unsigned target=0;
            for (unsigned bit=1;bit<image.w||bit<image.h;bit<<=1) {
                if (bit<image.w) { if(x&bit)source|=size_t(1)<<target;target++; }
                if (bit<image.h) { if(y&bit)source|=size_t(1)<<target;target++; }
            }
        }
        auto texel=Slice(image.pixels,source*bpp,bpp);
        if (bpp==1)texel=Slice(image.palette,size_t(texel[0])*4,4);
        std::copy_n(texel.begin(),4,out.begin()+(size_t(y)*image.w+x)*4);
    }
    return out;
}

// The named 3D archive families are supported by the reference's XML/archive
// research. Their alpha can be material data. Never infer this from alpha values
// or an embedded texture name; preserve independent channels for these scopes.
static bool MaterialSource(const std::string& label) {
    auto path=Lower(label);size_t at=0;
    while(at<path.size()) {
        auto end=path.find_first_of("/:\\",at);if(end==std::string::npos)end=path.size();
        auto component=path.substr(at,end-at);
        if(component.ends_with(".viv")) {
            component.resize(component.size()-4);
            for(const char* scope:{"v2ip","v2cp","v2bg","icrowd","cincrowd","v2cm"}) {
                std::string prefix=scope;
                if(component==prefix||component.starts_with(prefix+"_"))return true;
            }
        }
        at=end+1;
    }
    return false;
}

// Separable Lanczos3, clamped edges. Opacity/UI uses premultiplied filtering and
// straight BGRA output; named material archives preserve independent channels.
struct Tap { int first=0;std::vector<float> weights; };
static std::vector<Tap> Taps(unsigned source,unsigned target) {
    std::vector<Tap> taps(target);constexpr double pi=3.14159265358979323846;
    double filter_scale=std::max(1.,double(source)/target);
    auto sinc=[&](double x){return std::abs(x)<1e-10?1.:std::sin(pi*x)/(pi*x);};
    for (unsigned i=0;i<target;i++) {
        double center=(i+.5)*source/target-.5;
        int first=int(std::ceil(center-3*filter_scale)),last=int(std::floor(center+3*filter_scale));
        auto& tap=taps[i];tap.first=first;double sum=0;
        for(int x=first;x<=last;x++) { double d=(x-center)/filter_scale;
            double weight=std::abs(d)<3?sinc(d)*sinc(d/3):0;tap.weights.push_back(float(weight));sum+=weight; }
        for (float& v:tap.weights)v=float(v/sum);
    }
    return taps;
}
static Bytes Resize(View source,unsigned w,unsigned h,unsigned scale,bool premultiplied=true) {
    Check(source.size()==size_t(w)*h*4,"Invalid BGRA image");unsigned ow=w*scale,oh=h*scale;
    auto xs=Taps(w,ow),ys=Taps(h,oh);std::vector<float> tmp(size_t(ow)*h*4);Bytes out(size_t(ow)*oh*4);
    for (unsigned y=0;y<h;y++)for(unsigned x=0;x<ow;x++) {
        float values[4]{};const auto& tap=xs[x];
        for (unsigned n=0;n<tap.weights.size();n++) {
            int sx=std::clamp(tap.first+int(n),0,int(w)-1);auto* p=source.data()+(size_t(y)*w+sx)*4;
            float a=premultiplied?p[3]/255.f:1.f,weight=tap.weights[n];
            for (unsigned c=0;c<3;c++)values[c]+=p[c]*a*weight;values[3]+=p[3]*weight;
        }
        std::copy_n(values,4,tmp.data()+(size_t(y)*ow+x)*4);
    }
    for (unsigned y=0;y<oh;y++)for(unsigned x=0;x<ow;x++) {
        float values[4]{};const auto& tap=ys[y];
        for (unsigned n=0;n<tap.weights.size();n++) {
            int sy=std::clamp(tap.first+int(n),0,int(h)-1);auto* p=tmp.data()+(size_t(sy)*ow+x)*4;
            for(unsigned c=0;c<4;c++)values[c]+=p[c]*tap.weights[n];
        }
        auto* pixel=out.data()+(size_t(y)*ow+x)*4;float alpha=std::clamp(values[3],0.f,255.f);
        for(unsigned c=0;c<3;c++) {
            float value=premultiplied?(alpha>.001f?values[c]*255/alpha:0):values[c];
            pixel[c]=uint8_t(std::clamp(std::lround(value),0l,255l));
        }
        pixel[3]=uint8_t(std::lround(alpha));
    }
    return out;
}

struct Job { std::string id,label;uint32_t w,h;Bytes pixels;std::vector<std::string> aliases;bool material=false; };
struct Pool {
    std::deque<Job> queue;std::vector<std::thread> workers;std::condition_variable available,room;
    std::mutex mutex,results_mutex;bool done=false;std::atomic<bool> failed{false};std::string error;unsigned scale,total;
    fs::path output;std::string folder;std::atomic<unsigned> completed{0},resumed{0};std::atomic<uint64_t> bytes{0};
    std::map<std::string,std::string> index,checksums,prior,policies,prior_policies;
    unsigned material_count=0;
    Pool(fs::path out,unsigned s,unsigned count,unsigned threads):scale(s),total(count),output(std::move(out)) {
        folder="textures/lanczos3-material-v3-"+std::to_string(scale)+"x/";fs::create_directories(output/folder);
        std::ifstream old(output/("checksums-"+std::to_string(scale)+"x.tsv"));std::string line;
        while (std::getline(old,line)) { auto tab=line.find('\t');if(tab!=std::string::npos)prior.emplace(line.substr(0,tab),line.substr(tab+1)); }
        std::ifstream old_policies(output/"policies.tsv");
        while (std::getline(old_policies,line)) { auto tab=line.find('\t');if(tab!=std::string::npos)prior_policies.emplace(line.substr(0,tab),line.substr(tab+1)); }
        for(unsigned n=0;n<threads;n++)workers.emplace_back([this]{ Worker(); });
    }
    ~Pool() { Finish(); }
    void Finish() {
        { std::lock_guard lock(mutex);done=true; }available.notify_all();
        for(auto& worker:workers)if(worker.joinable())worker.join();
    }
    void Submit(Job job) {
        std::unique_lock lock(mutex);room.wait(lock,[&]{return queue.size()<12||failed;});
        Check(!failed,"HD worker failed");queue.push_back(std::move(job));available.notify_one();
    }
    void Worker() {
        try {
            Com com;auto factory=Factory();
            while(true) {
                Job job;
                {std::unique_lock lock(mutex);available.wait(lock,[&]{return done||failed||!queue.empty();});
                 if(failed||(queue.empty()&&done))return;job=std::move(queue.front());queue.pop_front();room.notify_one();}
                std::string relative=folder+job.id+".png";auto destination=output/relative;
                auto previous=prior.find(job.id);bool reuse=false;
                auto previous_policy=prior_policies.find(job.id);
                const std::string policy=job.material?"channels":"opacity";
                if(previous!=prior.end()&&previous_policy!=prior_policies.end()&&previous_policy->second==policy&&fs::is_regular_file(destination))
                    reuse=HashFile(destination)==previous->second;
                if (!reuse) {
                    auto pixels=Resize(job.pixels,job.w,job.h,scale,!job.material);auto pending=destination;pending+=L".pending";
                    Png(factory.Get(),pending,job.w*scale,job.h*scale,pixels);AtomicReplace(pending,destination);
                } else resumed++;
                std::string checksum=reuse?previous->second:HashFile(destination);bytes+=fs::file_size(destination);
                {std::lock_guard lock(results_mutex);
                 checksums[job.id]=checksum;index[job.id]=relative;
                 policies[job.id]=policy;if(job.material)material_count++;
                 for(const auto& alias:job.aliases)index.try_emplace(alias,relative);
                 unsigned n=++completed;std::cout<<"progress "<<n<<' '<<total<<std::endl;}
            }
        } catch(const std::exception& e) {
            {std::lock_guard lock(mutex);failed=true;error=e.what();}room.notify_all();available.notify_all();
        }
    }
};

static void WriteMap(const fs::path& path,const std::map<std::string,std::string>& map) {
    auto pending=path;pending+=L".pending";std::ofstream out(pending,std::ios::binary);
    for(const auto& [key,value]:map)out<<key<<'\t'<<value<<'\n';out.close();Check(bool(out),"Cannot write pack index");AtomicReplace(pending,path);
}
static std::map<std::string,std::vector<std::string>> RuntimeAliases() {
    // Only exact full-RGBA matches established by the MIT reference pilot.
    // Keys below are identities, never original game bytes or artwork.
    return {
        {"3b2f7c898dcdee40a9f1705689a5fbe129442fdc54e026424154a14e38edcb4e",{"7d925ac0d04772ba127c1a59637c46574014caaca2529f6ebe1a716c974649ba"}},
        {"f39e28173230de419dc68d3782e6f8c18c014c869f1cd541e69988a710e72383",{"dfb1c3cf7c50c2437a8ef2f590df50409e671ce00e20ef459215399c08f83123"}},
        {"81f4d91059214d34496607c9950b99d916c54bf5fc2c2fd03cdcd94bd5764479",{"e2f7346233831e602345dd85ae1d92bbdef7dfc6830cc5c5469b3d78911bb497"}},
        {"e4d2a5ae91c6f4dd1fe3a2f7da0bf0ed03ee1934008a0e45d27d2ab334ff069b",{"80dca2b0210724ea70a105b0b5eebd17e36776001d4eaf9719d605df9754bc4b"}},
        {"8138b261bb3a97f9eb31181a6215925d01d4ebd48a97235f5115156a2cfd1864",{"33548b520a224937a0074194b51e02ca1b9e5808cda07574fd6574c8b3edf43c"}},
        {"ea2cfbbfc77331545e60a8c35ab5fd1b6799ebd0f3cab215ffa84529c68cd091",{"ae55367b277c4795ff1c03dcddd0de75a45a57c4b4cb14f9dc79821dcd01570f"}},
        {"a24818088936878fd8fbe50e72bc9933d0b8608b9ef6d2dc6eb62510faa00fd0",{"fd15751bd8f709f48d7e28b2d804ba78368309773d24334e177001320d69bcee"}},
        {"8740c07e6f6cdc76e2cc7e038b1f31913c33b3bb3a67ff7dd4ef386ba0d6e1b7",{"17f88751ee59d694f36d4bda7416efc6fdc4ced00b57647f2fb256729b84c961"}},
        {"5806457da621685baafccba59b365e4c97d8dfc49242db1961bf26335b8c04a3",{"f4adc52d3644429900b02381d613c7222852c3b50dc391542b0099193d150206"}},
        {"0f02fbce5109ae99d0177f9a87f2a02298b459693bf9c0b81300224a6f100257",{"f91a3609a4e8977386ee159dcd7cb47831fd37afa6b165f81a0acc4c324fddfc"}},
        {"0b9e3a3219e30787e9bfd9ea44279679277cdcd2fb6766891364eea55542f0fd",{"126ec35e3cbef680cd8d662a182480b9dbe340c75757b36568ddcb034e793c4f"}},
        {"891c25bcc598f68155a8f9bd8860f6f053e2e6e79d609c59cb3c92e1615de0c1",{"e8f8ceb746e4f958c56ae60a5a2ff196685f8ef2bca107fcd8b12e290b68cc35"}},
        {"411c2e340ec8136eaa2059a66ff7901f5dfcfc45c6fcb439fd76d8c1c604deee",{"179b7de7b2128c95bcc7818e5a59d3505fc9a8175f7c25b88aeb2d8e223e5271"}}
    };
}

static void SelfTest();
int wmain(int argc,wchar_t** argv) {
    try {
        fs::path input,output;unsigned scale=4,limit=UINT32_MAX,workers=std::clamp(std::thread::hardware_concurrency()/2,1u,8u);
        std::string select;
        for(int i=1;i<argc;i++) {
            std::wstring arg=argv[i];if(arg==L"--self-test") {SelfTest();return 0;}
            if(arg==L"--help") {
                std::cout<<"defjam_hdpack --iso game.iso|--root extracted --output hd [--scale 2|4] [--workers 1..16] [--limit N] [--select substring]\n";return 0;
            }
            Check(i+1<argc,"Missing option value");auto value=argv[++i];
            if(arg==L"--iso"||arg==L"--root")input=value;else if(arg==L"--output")output=value;
            else if(arg==L"--scale")scale=std::stoul(value);else if(arg==L"--limit")limit=std::stoul(value);
            else if(arg==L"--workers")workers=std::stoul(value);else if(arg==L"--select")select=Lower(fs::path(value).string());
            else throw std::runtime_error("Unknown option");
        }
        Check(!input.empty()&&!output.empty(),"Use --iso or --root, and --output");
        Check((scale==2||scale==4)&&workers>=1&&workers<=16&&limit>0,"Invalid scale, workers or limit");
        auto start=std::chrono::steady_clock::now();Sources sources(input);std::set<std::string> seen;
        std::map<std::string,bool> material_sources;
        std::vector<const SourceFile*> files;unsigned skipped=0,unresolved=0;auto aliases=RuntimeAliases();
        for(const auto& file:sources.files) {
            auto ext=Lower(fs::path(file.name).extension().string());
            if(ext!=".viv"&&ext!=".xsh"&&ext!=".mad")continue;
            if(!select.empty()&&Lower(file.name).find(select)==std::string::npos)continue;
            if(file.size>MaxBlob) {skipped++;continue;}files.push_back(&file);
        }
        std::cout<<"Inventorying "<<files.size()<<" archives (bounded to 128 MiB each)"<<std::endl;
        auto process=[&](Pool* pool) {
            for(const auto* file:files) {
                if(seen.size()>=limit)break;
                try {
                    auto blob=sources.Load(*file);
                    Walk(blob,file->name,[&](View b,const std::string& label){
                        if(!Magic(b,"SHPX")||seen.size()>=limit)return;
                        Check(b.size()>=16&&U32(b,4)==b.size(),"Invalid SHPX extent");uint32_t count=U32(b,8);
                        Check(count<=65536&&16ull+count*8<=b.size(),"Invalid SHPX directory");
                        for(uint32_t n=0;n<count&&seen.size()<limit;n++) {
                            try {
                                size_t offset=U32(b,20+n*8);Slice(b,offset,1);if(b[offset]==0x2a)continue;
                                auto image=Shpx(b,offset);
                                auto id=defjam::hd::TextureId(image.fmt,image.w,image.h,image.pixels,image.palette);
                                if(!pool)material_sources[id]=material_sources[id]||MaterialSource(label);
                                if(!seen.insert(id).second)continue;
                                if(pool) {
                                    Job job{id,label,image.w,image.h,Decode(image),{}};
                                    job.material=material_sources.at(id);
                                    job.aliases.push_back(defjam::hd::TextureId(0x12,image.w,image.h,job.pixels));
                                    if(!image.palette.empty()&&image.palette.size()<1024) {
                                        Bytes palette(image.palette.begin(),image.palette.end());palette.resize(1024);
                                        job.aliases.push_back(defjam::hd::TextureId(image.fmt,image.w,image.h,image.pixels,palette));
                                    }
                                    auto found=aliases.find(id);if(found!=aliases.end())job.aliases.insert(job.aliases.end(),found->second.begin(),found->second.end());
                                    pool->Submit(std::move(job));
                                }
                            }catch(const std::exception& e) {
                                if(pool&&pool->failed)throw;
                                if(!pool) {unresolved++;std::cerr<<"Unresolved "<<label<<" #"<<n<<": "<<e.what()<<'\n';}
                            }
                        }
                    });
                }catch(const std::exception& e) {
                    if(pool&&pool->failed)throw;
                    if(!pool) {unresolved++;std::cerr<<"Skipped "<<file->name<<": "<<e.what()<<'\n';}
                }
            }
        };
        process(nullptr);unsigned total=unsigned(seen.size());Check(total>0,"No supported static textures found");seen.clear();
        std::cout<<"Generating "<<total<<" unique textures, Lanczos "<<scale<<"x, "<<workers<<" workers"<<std::endl;
        std::cout<<"progress 0 "<<total<<std::endl;Pool pool(output,scale,total,workers);process(&pool);pool.Finish();
        Check(!pool.failed,pool.error.c_str());Check(pool.completed==total,"Incomplete HD generation");
        WriteMap(output/("checksums-"+std::to_string(scale)+"x.tsv"),pool.checksums);
        WriteMap(output/"policies.tsv",pool.policies);
        WriteMap(output/"index.tsv",pool.index);
        double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        std::ostringstream summary;summary<<"HD summary textures="<<pool.completed<<" bytes="<<pool.bytes<<" seconds="<<seconds<<
            " skipped="<<skipped<<" unresolved="<<unresolved<<" resumed="<<pool.resumed<<" scale="<<scale<<
            " material="<<pool.material_count<<" opacity="<<(total-pool.material_count);
        std::ofstream receipt(output/"pack.txt",std::ios::binary);
        receipt<<"DefJamHD schema=1 recipe=lanczos3-material-v3 hash=XRTEX01 png=BGRA8\n"
            "alpha=independent-in-v2ip-v2cp-v2bg-icrowd-cincrowd-v2cm;premultiplied-opacity-elsewhere\n"<<summary.str()<<'\n';
        receipt.close();Check(bool(receipt),"Cannot write HD receipt");std::cout<<summary.str()<<std::endl;
        return 0;
    }catch(const std::exception& e) {std::cerr<<"HD error: "<<e.what()<<std::endl;return 1;}
}

static void SelfTest() {
    using namespace defjam::hd;
    Sha256 empty;Check(empty.Finish()=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","SHA256 empty fixture failed");
    Sha256 abc;constexpr uint8_t text[]{'a','b','c'};abc.Update(text);
    Check(abc.Finish()=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA256 abc fixture failed");
    Sha256 long_sha;const std::string long_text="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    for(char c:long_text) {uint8_t byte=uint8_t(c);long_sha.Update(View(&byte,1));}
    Check(long_sha.Finish()=="248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1","SHA256 block-boundary fixture failed");
    Bytes pixels{0,0,255,255,255,0,0,0};auto id=TextureId(0x12,2,1,pixels);
    // Independently calculated with .NET SHA256 over the upstream Python recipe.
    Check(id=="ddf6eb4c97fc164f62bfcb5a935224d94f4e334373c012a3107e763db462010c","XRTEX01 known-answer fixture failed");
    Check(id!=TextureId(0x06,2,1,pixels)&&id!=TextureId(0x12,1,2,pixels),"Texture descriptor fixture failed");
    Bytes palette{0,0,255,255};auto indexed=TextureId(0x0b,1,1,View(pixels).first(1),palette);palette[2]=0;
    Check(indexed!=TextureId(0x0b,1,1,View(pixels).first(1),palette),"Palette identity fixture failed");
    auto resized=Resize(pixels,2,1,4);Check(resized.size()==128,"Lanczos dimensions fixture failed");
    for(size_t n=0;n<resized.size();n+=4)Check(resized[n]==0&&resized[n+1]==0,"Premultiplied alpha fixture failed");
    Bytes material_pixel{25,100,200,0};auto material_image=Resize(material_pixel,1,1,4,false);
    for(size_t n=0;n<material_image.size();n+=4)Check(std::equal(material_pixel.begin(),material_pixel.end(),material_image.begin()+n),"Material RGB at alpha zero was lost");
    Check(MaterialSource("assets/v2ip_cin.viv::assets/V2IP_113B.viv::a.xsh")&&MaterialSource("assets/v2cp_0.viv::part.xsh")&&
        !MaterialSource("screens/menu.viv::preview_v2ip.xsh")&&!MaterialSource("assets/notv2ip.viv::a.xsh"),"Material archive scope fixture failed");
    Bytes bc{0x00,0xf8,0x00,0x00,0,0,0,0},decoded;
    Check(DecodeBC(0x0c,4,4,bc,decoded)&&decoded[2]==255&&decoded[0]==0&&decoded[3]==255,"BC1 red fixture failed");
    Bytes expansion{4,0,0,0,0,0,0,0};
    Check(DecodeBC(0x0c,4,4,expansion,decoded)&&decoded[0]==33,"BC endpoint bit replication fixture failed");
    Check(!DecodeBC(0x0c,4,4,View(bc).first(7),decoded),"Truncated BC accepted");
    Bytes bc2(16,0xff);std::copy(bc.begin(),bc.end(),bc2.begin()+8);bc2[0]=0x50;
    Check(DecodeBC(0x0e,4,4,bc2,decoded)&&decoded[2]==255&&decoded[3]==0&&decoded[7]==85,"BC2 alpha fixture failed");
    Bytes bc3(16,0);std::copy(bc.begin(),bc.end(),bc3.begin()+8);bc3[0]=255;bc3[1]=0;bc3[2]=8;
    Check(DecodeBC(0x0f,4,4,bc3,decoded)&&decoded[2]==255&&decoded[3]==255&&decoded[7]==0,"BC3 alpha fixture failed");
    Bytes ref{0x10,0xfb,0,0,3,0xff,'a','b','c'};
    Check(Refpack(ref)==Bytes({'a','b','c'}),"RefPack literal fixture failed");
    ref[5]=0x80;bool rejected=false;try{Refpack(ref);}catch(...){rejected=true;}Check(rejected,"Malformed RefPack accepted");
    Bytes backref{0x10,0xfb,0,0,7,3,0,'a','b','c',0xfd,'d'};
    Check(Refpack(backref)==Bytes({'a','b','c','c','c','c','d'}),"RefPack overlapping backreference failed");
    auto put=[](Bytes& b,size_t at,uint32_t v,unsigned bytes=4) {for(unsigned n=0;n<bytes;n++)b[at+n]=uint8_t(v>>(8*n));};
    Bytes shpx(72,0);std::copy_n("SHPX",4,shpx.begin());put(shpx,4,72);put(shpx,8,1);put(shpx,20,24);
    shpx[24]=0x7d;put(shpx,28,4,2);put(shpx,30,2,2);put(shpx,36,0x2000);
    for(unsigned n=0;n<8;n++) {shpx[40+n*4]=uint8_t(n);shpx[43+n*4]=255;}
    auto swizzled=Decode(Shpx(shpx,24));
    Check(swizzled[0]==0&&swizzled[8]==4&&swizzled[16]==2&&swizzled[24]==6,"SHPX Morton layout fixture failed");
    Bytes big(36,0);std::copy_n("BIGF",4,big.begin());big[11]=1;big[19]=32;big[23]=4;big[24]='x';big[25]='.';big[26]='y';
    std::copy_n("test",4,big.begin()+32);unsigned leaves=0;
    Walk(big,"synthetic",[&](View b,const std::string& label){Check(Magic(b,"test")&&label=="synthetic::x.y","BIG member fixture failed");leaves++;});
    Check(leaves==1,"BIG traversal fixture failed");
    Com com;auto factory=Factory();auto file=fs::temp_directory_path()/(L"defjam-hd-selftest-"+std::to_wstring(GetCurrentProcessId())+L".png");
    Png(factory.Get(),file,2,1,pixels);
    ComPtr<IWICBitmapDecoder> decoder;Hr(factory->CreateDecoderFromFilename(file.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder),"PNG fixture open failed");
    ComPtr<IWICBitmapFrameDecode> frame;Hr(decoder->GetFrame(0,&frame),"PNG fixture frame failed");
    ComPtr<IWICFormatConverter> converter;Hr(factory->CreateFormatConverter(&converter),"PNG fixture converter failed");
    Hr(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom),"PNG fixture channels failed");
    Bytes roundtrip(8);Hr(converter->CopyPixels(nullptr,8,8,roundtrip.data()),"PNG fixture read failed");
    converter.Reset();frame.Reset();decoder.Reset();fs::remove(file);Check(roundtrip==pixels,"PNG BGRA/alpha roundtrip failed");
    std::cout<<"HD self-test passed: SHA256/XRTEX01 known answers, palette identity, BC1/2/3 alpha, BIG/RefPack bounds, SHPX Morton, Lanczos opacity/material alpha policies, PNG BGRA roundtrip\n";
}
