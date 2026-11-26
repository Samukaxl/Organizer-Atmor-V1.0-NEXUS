#include "full-diagnostics.hpp"
#include <cassert>
#include <iostream>
string fixture(vector<string>masters){string b;for(auto&m:masters){b+="MAST";size_t n=m.size()+1;b+=(char)n;b+=(char)(n>>8);b+=m;b+='\0';}string h(24,'\0');h.replace(0,4,"TES4");for(int i=0;i<4;i++)h[4+i]=(char)(b.size()>>(i*8));return h+b;}
int main(){auto root=fs::temp_directory_path()/"samuk-full-test";fs::remove_all(root);auto p=root/"profiles"/"Default"/"modlist.txt",m=root/"mods",data=root/"Data",ow=root/"overwrite";
 for(auto&dir:{p.parent_path(),m/"Base",m/"Translation",data,ow})fs::create_directories(dir);
 writeFile(p,"+Translation\n+Base\n");writeFile(p.parent_path()/"plugins.txt","*patch.esp\n");
 writeFile(m/"Base"/"patch.esp",fixture({"good.esm"}));writeFile(m/"Translation"/"patch.esp",fixture({"middle.esm"}));writeFile(m/"Base"/"middle.esm",fixture({"gone.esm"}));
 auto d=diagnose(p,m,data,"unknown","","",ow);bool missing=false;for(auto&r:d.rows)if(r.item=="gone.esm"&&r.status=="AUSENTE")missing=true;assert(missing);
 completeDiagnostics(d,p,m,data,ow,"MISSING MASTERS (FATAL):\n missing: gone.esm\n","old-crash.log");auto report=diagnosticText(d);assert(report.find("MAST DIFERENTES")!=string::npos&&report.find("ERRO REGISTRADO NO LOG")!=string::npos&&report.find("ARQUIVO DO PERFIL")!=string::npos);
 writeFile(ow/"patch.esp",fixture({}));d=diagnose(p,m,data,"unknown","","",ow);for(auto&r:d.rows)assert(!(r.item=="gone.esm"&&r.status=="AUSENTE"));
 completeDiagnostics(d,p,m,data,ow);assert(diagnosticText(d).find("Vencedor: overwrite")!=string::npos);assert(readFile(p)=="+Translation\n+Base\n");
 Diagnostics generic;addCrashEvidence(generic,"Unhandled exception EXCEPTION_ACCESS_VIOLATION\n wheeler.dll\n","crash.log");bool uncertain=false;for(auto&r:generic.rows)if(r.status=="INCONCLUSIVO")uncertain=true;assert(uncertain);
 fs::remove_all(root);std::cout<<"PASS transitive masters, effective overwrite, translation MAST differences, historical crash, no false attribution, read-only report\n";}
