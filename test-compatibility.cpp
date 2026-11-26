#include "compatibility.hpp"
#include "discovery.hpp"
#include <cassert>
#include <iostream>
#include <random>
void put32(string&s,size_t p,uint32_t v){for(int i=0;i<4;i++)s.at(p+i)=(char)(v>>(8*i));}
void put16(string&s,size_t p,uint32_t v){s.at(p)=v&255;s.at(p+1)=(v>>8)&255;}
string peFixture(){string s(2048,'\0');s.replace(0,2,"MZ");put32(s,60,128);s.replace(128,4,string("PE\0\0",4));put16(s,132,0x8664);put16(s,134,1);put32(s,136,1700000000);put16(s,148,240);put16(s,152,0x20b);put32(s,260,16);put32(s,264,4096);put32(s,268,128);put32(s,404,4096);put32(s,408,1536);put32(s,412,512);put32(s,532,1);put32(s,536,1);put32(s,540,4136);put32(s,544,4140);put32(s,548,4144);put32(s,552,4352);put32(s,556,4160);put16(s,560,0);s.replace(576,18,"SKSEPlugin_Version");put32(s,768,1);put32(s,768+776,5);put32(s,768+780,packedRuntime("1.6.1170"));return s;}
int main(int argc,char**argv){assert(packedRuntime("1.7.104.0")==0x01070680);assert(!packedRuntime("hello"));assert(!packedRuntime("1.6.99999"));auto s=peFixture();auto d=dllDeclaration(s);assert(d.found&&d.flags==5&&d.runtimes.size()==1);auto v=evaluateDll(d,packedRuntime("1.7.104"));assert(v.blocked&&v.requiresAddress&&v.reason.find("compilacao")!=string::npos);d.extra=2;assert(!evaluateDll(d,packedRuntime("1.7.104")).blocked);d.flags=0;d.runtimes={packedRuntime("1.6.1170")};assert(evaluateDll(d,packedRuntime("1.7.104")).blocked);assert(!evaluateDll(d,packedRuntime("1.6.1170")).blocked);d.flags=1;d.extra=0;assert(evaluateDll(d,packedRuntime("1.6.1170")).blocked);d.extra=1;assert(!evaluateDll(d,packedRuntime("1.6.1170")).blocked);d.flags=0x80000000;assert(evaluateDll(d,packedRuntime("1.7.104")).status=="NAO VERIFICADO");assert(evaluateDll(d,packedRuntime("1.8.1")).status=="NAO VERIFICADO");
 bool failed=false;try{dllDeclaration(s.substr(0,800));}catch(...){failed=true;}assert(failed);put32(s,556,0xfffffff0);failed=false;try{dllDeclaration(s);}catch(...){failed=true;}assert(failed);
 std::mt19937 gen(7);for(int i=0;i<500;i++){string fuzz(gen()%3000,'\0');for(char&c:fuzz)c=(char)gen();try{dllDeclaration(fuzz);}catch(...) {}}
 auto ini=iniValues("[General]\ngamePath=@ByteArray(D:\\\\Steam\\\\Skyrim Special Edition)\nselected_profile=@ByteArray(Default)\n[Settings]\nmod_directory=%BASE_DIR%/mods\n");assert(ini["selected_profile"]=="Default");assert(ini["gamepath"]=="D:\\Steam\\Skyrim Special Edition");auto vdf=vdfStrings("\"libraryfolders\" { \"0\" { \"path\" \"D:\\\\Steam Library\" }}");assert(vdf.back()=="D:\\Steam Library");assert(resolveMOPath("%BASE_DIR%/mods",fs::path("/test"))==fs::path("/test/mods"));
 if(argc>1){auto real=inspectDll(argv[1]);assert(real.found&&real.flags==5&&real.extra==2&&real.runtimes[0]==packedRuntime("1.7.104"));}
 std::cout<<"PASS PE parser, runtime rules, malformed DLLs, MO2 paths and Steam VDF\n";
}
