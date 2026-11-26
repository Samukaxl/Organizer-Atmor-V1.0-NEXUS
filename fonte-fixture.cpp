#include <cstdint>
struct Version{uint32_t dataVersion,pluginVersion;char name[256],author[256],email[252];uint32_t extra,flags,runtimes[16],minimum;};
extern "C" { __declspec(dllexport) Version SKSEPlugin_Version={1,1,"Test","Test","",2,5,{0x01070680},0x02030010}; }
