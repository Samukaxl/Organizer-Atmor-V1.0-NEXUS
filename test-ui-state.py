"""Regression: execute the real Win32 path/timer handlers with synchronous EN_CHANGE stubs.
Does not replace interactive Windows validation. Requires Python 3 and g++.
"""
from pathlib import Path
import subprocess, tempfile
s=Path(__file__).with_name('main.cpp').read_text()
cancel=s[s.index('void cancelPendingDiscovery()'):s.index('void chooseDetectedProfile(')]
normalize=s[s.index(' p=fs::absolute(p).lexically_normal();'):s.index('\n bool online=',s.index('void analyze()'))]
enchange=s.split('case WM_COMMAND:{int id=LOWORD(wp);',1)[1].split('if(busy)return 0;',1)[0]
timer=s.split('case WM_TIMER:',1)[1].split('\n',1)[0]
code=r'''
#include <cassert>
#include <filesystem>
#include <string>
namespace fs=std::filesystem;
using HWND=int;
const int win=1,profileBox=101,modsBox=102,applyButton=106;
const int FALSE=0,EN_CHANGE=0x300;
#define HIWORD(x) ((x)>>16)
bool ready=true,enabled=true,updatingDiscovery=false,discoveryPending=false,busy=false;
int timers=0,discoveries=0;
std::wstring fields[103],gameExe,gameVersion,importedLog;
void KillTimer(int,int){timers=0;}
void SetTimer(int,int,int,void*){timers=1;}
void EnableWindow(int,int enabledValue){enabled=enabledValue;}
std::wstring value(HWND h){return fields[h];}
int change(int id,int wp){HANDLER return 0;}
void SetWindowTextW(HWND h,const wchar_t* text){fields[h]=text;change(h,EN_CHANGE<<16);}
void SetWindowTextW(HWND h,const char* text){SetWindowTextW(h,fs::path(text).wstring().c_str());}
HELPERS
void beginDiscovery(){cancelPendingDiscovery();++discoveries;}
int tick(){int wp=1;TIMER}
void normalize(){auto p=fs::path(value(profileBox)),m=fs::path(value(modsBox));NORMALIZE}
int main(){
 fields[101]=L"/profile/modlist.txt";fields[102]=L"/mods";
 // Same selected paths must not emit EN_CHANGE and destroy a completed preview.
 setPathIfChanged(profileBox,fs::path(fields[101]));setPathIfChanged(modsBox,fs::path(fields[102]));
 assert(ready&&enabled&&!discoveryPending);
 // User edit schedules discovery and invalidates preview.
 SetWindowTextW(profileBox,L"/profile/./modlist.txt");assert(!ready&&!enabled&&discoveryPending&&timers);
 // Generate preview normalizes internally without scheduling another discovery.
 normalize();assert(!discoveryPending&&!timers&&fields[101]==L"/profile/modlist.txt");
 ready=true;enabled=true;tick();assert(ready&&enabled&&discoveries==0);
 // A later actual user edit must still invalidate and run discovery.
 SetWindowTextW(profileBox,L"/other/modlist.txt");assert(!ready&&!enabled&&discoveryPending);
 tick();assert(discoveries==1&&!discoveryPending);tick();assert(discoveries==1);
}
'''.replace('HANDLER',enchange).replace('HELPERS',cancel).replace('TIMER',timer).replace('NORMALIZE',normalize)
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp';p.write_text(code)
 subprocess.run(['g++','-std=c++17',str(p),'-o',d+'/test'],check=True)
 subprocess.run([d+'/test'],check=True)
print('OK: preview survives normalization, rediscovery of unchanged paths and stale timer; real edits invalidate it.')
