#pragma once
#include "i18n.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs=std::filesystem;
using std::string; using std::vector;
inline string readFile(const fs::path&p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Nao foi possivel ler: "+p.u8string());return string(std::istreambuf_iterator<char>(f),{});}
inline void writeFile(const fs::path&p,const string&s){std::ofstream f(p,std::ios::binary|std::ios::trunc);if(!f)throw std::runtime_error("Nao foi possivel gravar: "+p.u8string());f.write(s.data(),s.size());f.close();if(!f)throw std::runtime_error("Falha ao gravar arquivo.");}
inline string low(string s){
#ifdef _WIN32
int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),s.size(),0,0);
if(n>0){std::wstring w(n,L' ');MultiByteToWideChar(CP_UTF8,0,s.data(),s.size(),w.data(),n);int m=LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_LOWERCASE,w.data(),n,0,0,0,0,0);if(m>0){std::wstring v(m,L' ');LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_LOWERCASE,w.data(),n,v.data(),m,0,0,0);int k=WideCharToMultiByte(CP_UTF8,0,v.data(),m,0,0,0,0);string out(k,' ');WideCharToMultiByte(CP_UTF8,0,v.data(),m,out.data(),k,0,0);return out;}}
#endif
for(char&c:s)if(c>='A'&&c<='Z')c+=32;return s;}
// Pure string normalization is idempotent, including extended UNC paths.
// Do not run lexically_normal/absolute over a path that already has an NT prefix.
inline std::wstring extendedWindowsAbsolute(std::wstring w){
 std::replace(w.begin(),w.end(),L'/',L'\\');
 const std::wstring prefix=L"\\\\?\\";
 if(w.rfind(prefix,0)==0)return w;
 if(w.rfind(L"\\\\",0)==0)return prefix+L"UNC\\"+w.substr(2);
 if(w.size()>=3&&w[1]==L':'&&w[2]==L'\\')return prefix+w;
 throw std::runtime_error("Esperado um caminho Windows absoluto.");
}
inline fs::path nativePath(const fs::path&p){
#ifdef _WIN32
 auto w=p.native();std::replace(w.begin(),w.end(),L'/',L'\\');
 if(w.rfind(L"\\\\?\\",0)==0)return fs::path(w);
 if(w.rfind(L"\\\\",0)==0||(w.size()>=3&&w[1]==L':'&&w[2]==L'\\'))return fs::path(extendedWindowsAbsolute(w));
 DWORD needed=GetFullPathNameW(w.c_str(),0,nullptr,nullptr);if(!needed)throw std::runtime_error("Nao foi possivel resolver caminho absoluto.");
 std::wstring full(needed,L' ');DWORD got=GetFullPathNameW(w.c_str(),needed,full.data(),nullptr);if(!got||got>=needed)throw std::runtime_error("Falha ao resolver caminho absoluto.");full.resize(got);return fs::path(extendedWindowsAbsolute(full));
#else
 return p;
#endif
}
#ifdef _WIN32
inline string winError(const string&operation,const fs::path&p,DWORD code){
 LPWSTR msg=nullptr;FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,code,0,(LPWSTR)&msg,0,nullptr);
 string detail;if(msg){int n=WideCharToMultiByte(CP_UTF8,0,msg,-1,nullptr,0,nullptr,nullptr);if(n>0){detail.resize(n);WideCharToMultiByte(CP_UTF8,0,msg,-1,detail.data(),n,nullptr,nullptr);detail.pop_back();}LocalFree(msg);}
 string hint="Confira se a pasta existe e permanece acessivel. Se instalou ou removeu mods, atualize o MO2 e gere uma nova analise.";
 if(code==ERROR_ACCESS_DENIED)hint="O Windows negou acesso. Confira as permissoes desta pasta e se os arquivos estao disponiveis localmente.";
 if(code==ERROR_FILENAME_EXCED_RANGE)hint="O caminho excedeu um limite do Windows. Esta versao usa caminhos longos; copie este relatorio para investigar o arquivo indicado.";
 return operation+"\r\n\r\nArquivo ou pasta: "+p.u8string()+"\r\nCodigo Windows: "+std::to_string(code)+"\r\nMotivo: "+detail+"\r\n"+hint;
}
#endif
inline bool linked(const fs::path&p){
#ifdef _WIN32
 auto path=nativePath(p);DWORD a=GetFileAttributesW(path.c_str());
 if(a==INVALID_FILE_ATTRIBUTES){DWORD code=GetLastError();throw std::runtime_error(winError("Nao foi possivel consultar o arquivo.",p,code));}
 return (a&FILE_ATTRIBUTE_REPARSE_POINT)!=0;
#else
 return fs::is_symlink(fs::symlink_status(p));
#endif
}
struct DirectoryItem{fs::path path;bool directory=false,link=false;};
inline vector<DirectoryItem> directoryItems(const fs::path&base){vector<DirectoryItem> result;
#ifdef _WIN32
 auto root=nativePath(base);auto text=root.native();if(text.back()!=L'\\')text+=L'\\';
 WIN32_FIND_DATAW data{};HANDLE handle=FindFirstFileW((text+L"*").c_str(),&data);
 if(handle==INVALID_HANDLE_VALUE){DWORD code=GetLastError();if(code==ERROR_FILE_NOT_FOUND)return result;throw std::runtime_error(winError("Nao foi possivel listar o diretorio.",base,code));}
 try{do{if(wcscmp(data.cFileName,L".")==0||wcscmp(data.cFileName,L"..")==0)continue;result.push_back({fs::path(text+data.cFileName),(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0,(data.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)!=0});}while(FindNextFileW(handle,&data));DWORD code=GetLastError();if(code!=ERROR_NO_MORE_FILES)throw std::runtime_error(winError("Falha ao listar diretorio.",base,code));}catch(...){FindClose(handle);throw;}FindClose(handle);
#else
 for(auto&e:fs::directory_iterator(base))result.push_back({e.path(),e.is_directory(),fs::is_symlink(e.symlink_status())});
#endif
 return result;
}
inline bool ends(const string&s,const string&t){return s.size()>=t.size()&&s.compare(s.size()-t.size(),t.size(),t)==0;}
// Enumeration uses the attributes returned by Windows itself, avoiding a second
// GetFileAttributes call for every file. Any unreadable directory stops the scan.
inline vector<string> scanFiles(const fs::path&directory){
 vector<string> files;
#ifdef _WIN32
 const auto root=nativePath(directory);
 DWORD rootAttrs=GetFileAttributesW(root.c_str());
 if(rootAttrs==INVALID_FILE_ATTRIBUTES){DWORD code=GetLastError();throw std::runtime_error(winError("Nao foi possivel acessar a pasta do mod.",directory,code));}
 if(!(rootAttrs&FILE_ATTRIBUTE_DIRECTORY))throw std::runtime_error("O caminho nao e uma pasta: "+directory.u8string());
 if(rootAttrs&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Pasta vinculada (junction/symlink) nao suportada: "+directory.u8string());
 vector<fs::path> pending={fs::path()};
 while(!pending.empty()){
   fs::path relative=pending.back();pending.pop_back();auto dir=root/relative;
   WIN32_FIND_DATAW entry{};HANDLE handle=FindFirstFileW((dir/L"*").c_str(),&entry);
   if(handle==INVALID_HANDLE_VALUE){DWORD code=GetLastError();if(code==ERROR_FILE_NOT_FOUND)continue;throw std::runtime_error(winError("Nao foi possivel listar a pasta.",directory/relative,code));}
   try{
     do{
       std::wstring name(entry.cFileName);if(name==L"."||name==L"..")continue;
       auto rel=relative/name;string key=low(rel.generic_u8string());
       if(ends(key,".mohidden"))continue;
       if(entry.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Link ou ponto de redirecionamento nao suportado: "+(directory/rel).u8string()+"\r\nA analise foi interrompida; nenhum mod sera ignorado silenciosamente.");
       if(entry.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)pending.push_back(rel);
       else if(key!="meta.ini")files.push_back(key);
     }while(FindNextFileW(handle,&entry));
     DWORD code=GetLastError();if(code!=ERROR_NO_MORE_FILES)throw std::runtime_error(winError("A leitura da pasta foi interrompida.",directory/relative,code));
   }catch(...){FindClose(handle);throw;}FindClose(handle);
 }
#else
 if(!fs::is_directory(directory))throw std::runtime_error("Pasta de mod ausente: "+directory.u8string());
 if(linked(directory))throw std::runtime_error("Pasta vinculada nao suportada: "+directory.u8string());
 for(auto it=fs::recursive_directory_iterator(directory);it!=fs::recursive_directory_iterator();++it){
   auto key=low(it->path().lexically_relative(directory).generic_u8string());
   if(ends(key,".mohidden")){if(it->is_directory())it.disable_recursion_pending();continue;}
   if(linked(it->path()))throw std::runtime_error("Link nao suportado: "+it->path().u8string());
   if(it->is_regular_file()&&key!="meta.ini")files.push_back(key);
 }
#endif
 return files;
}
inline vector<string> lines(string s){if(s.compare(0,3,"\xef\xbb\xbf")==0)s.erase(0,3);vector<string>r;std::istringstream f(s);string a;while(std::getline(f,a)){if(!a.empty()&&a.back()=='\r')a.pop_back();if(!a.empty())r.push_back(a);}return r;}
inline bool safeName(const string&s){return !s.empty()&&s!="."&&s!=".."&&s.find_first_of("/\\:\r\n\t") == string::npos;}
const vector<string> cats={"Requisitos e bibliotecas","Texturas e modelos","Ambiente e iluminacao","Cidades","Missoes e aventuras","Gameplay e economia","Magia","Armas e equipamentos","Combate","Animacoes","Camera e movimentacao","Audio","Interface","NPCs e personagem","Traducoes e legendas","Dublagens","Saidas de ferramentas","Revisar - nao classificado","Base e correcoes"};
struct Mod{string name;char state;int cat=17;};
struct Parsed{vector<Mod> mods;vector<string>official,seps,comments;};
inline Parsed parse(const string&data){Parsed p;auto l=lines(data);std::set<string> seen;for(auto&s:l){if(s[0]=='#'){p.comments.push_back(s);continue;}if(s[0]!='+'&&s[0]!='-'&&s[0]!='*')throw std::runtime_error("Linha invalida no modlist: "+s);string n=s.substr(1);if(!safeName(n)&&s[0]!='*')throw std::runtime_error("Nome de mod invalido.");if(!seen.insert(low(n)).second)throw std::runtime_error("Nome duplicado no modlist: "+n);if(s[0]=='*')p.official.push_back(s);else if(ends(n,"_separator"))p.seps.push_back(s);else p.mods.push_back({n,s[0],17});}std::reverse(p.mods.begin(),p.mods.end());std::reverse(p.official.begin(),p.official.end());std::reverse(p.seps.begin(),p.seps.end());if(p.mods.empty())throw std::runtime_error("Nenhum mod no arquivo.");return p;}
using Rules=std::map<string,int>;
inline Rules parseRules(const string&s){Rules r;for(auto&l:lines(s)){if(l[0]=='#')continue;auto tab=l.find('\t');if(tab==string::npos)throw std::runtime_error("Regra invalida (use numero TAB nome).");int c=std::stoi(l.substr(0,tab));if(c<0||c>18)throw std::runtime_error("Categoria invalida nas regras.");r[l.substr(tab+1)]=c;}return r;}
struct Inference{int cat=17;string reason="Sem evidencia suficiente";};
inline bool hasAny(const string&s,std::initializer_list<const char*>words){for(auto w:words)if(s.find(w)!=string::npos)return true;return false;}
#include "classification.hpp"
inline Inference inferCategory(const string&name,const vector<string>&files){
 string n=low(name);int voice=0,sound=0,anim=0,tex=0,mesh=0,ui=0,dll=0,scripts=0,plugins=0,archives=0;
 for(auto&f:files){if(f.rfind("sound/voice/",0)==0)voice++;else if(f.rfind("sound/",0)==0||f.rfind("music/",0)==0)sound++;
 if(ends(f,".hkx"))anim++;if(ends(f,".dds"))tex++;if(ends(f,".nif"))mesh++;
 if(f.rfind("interface/",0)==0&&ends(f,".swf"))ui++;
 if(ends(f,".dll"))dll++;if(ends(f,".pex")||ends(f,".psc"))scripts++;
 if(ends(f,".esp")||ends(f,".esm")||ends(f,".esl"))plugins++;
 if(ends(f,".bsa")||ends(f,".ba2"))archives++;}
 bool language=hasAny(n,{"pt-br","ptbr","pt br","portugues","português","brazilian","tradu","translation","translated","localization","legenda","traducción","traduccion","español","espanol","castellano","subtitles","subtítulos","subtitulos"});
 if(hasAny(n,{"dublag","dubbed","dubbing","doblaje"})||(language&&voice))return {15,"Indicador de dublagem no nome ou idioma + voz"};
 if(language)return {14,"Indicador de traducao no nome"};
 if(classificationPhrase(n,"output")||classificationPhrase(n,"saida")||classificationPhrase(n,"saída"))return {16,"Nome identifica saida de ferramenta"};
 auto known=catalogCategory(name);if(known.cat!=17)return known;
 // An installed plugin/DLL identifier can identify renamed mod folders.
 static const std::map<string,int> signatures={{"wyrmstooth.esp",4},{"missives.esp",4},{"bsheartland.esm",4},{"valhallacombat.dll",8},{"precision.dll",8},{"wheeler.dll",12},{"skee64.dll",12},{"lux.esp",2}};
 Inference signature;string matched;
 for(auto&f:files){auto base=f.substr(f.find_last_of("/\\")==string::npos?0:f.find_last_of("/\\")+1);auto it=signatures.find(base);if(it==signatures.end())continue;
  if(signature.cat!=17&&signature.cat!=it->second)return {17,"Identificadores de funcoes diferentes no mesmo mod: revisar categoria"};
  signature={it->second,"Identificador instalado reconhecido: "+base};}
 if(signature.cat!=17)return signature;
 // Semantic name evidence precedes incidental assets such as a menu or sound effect.
 const vector<std::pair<string,int>> words={{"quest",4},{"quests",4},{"adventure",4},{"new lands",4},{"weapon",7},{"weapons",7},{"armor",7},{"armors",7},{"armour",7},{"magic",6},{"spells",6},{"combat",8},{"camera",10},{"movement",10},{"animation",9},{"animations",9},{"interface",12},{"hud",12},{"menu",12},{"loading screens",12},{"weather",2},{"lighting",2},{"water",2},{"grass",2},{"trees",2},{"landscape",2},{"skin",13},{"hair",13},{"face",13},{"npc",13}};
 for(auto&w:words)if(classificationPhrase(n,w.first))return {w.second,"Termo de funcao no nome: "+w.first+" (revisar)"};
 // Extensions alone cannot establish the purpose of a mixed mod or an archive.
 if(!plugins&&!dll&&!scripts&&!archives){
  if((tex||mesh)&&!voice&&!sound&&!anim&&!ui)return {1,"Pacote de recursos visuais soltos, sem plugin/DLL/script/arquivo BSA"};
  if(ui&&!voice&&!sound&&!anim&&!mesh)return {12,"Pacote de interface SWF sem plugin/DLL/script"};
  if(anim&&!voice&&!sound&&!ui&&!tex)return {9,"Pacote de animacoes soltas, sem plugin/DLL/script"};
  if((voice||sound)&&!anim&&!ui&&!tex&&!mesh)return {11,"Pacote de audio solto; idioma nao confirmado"};
 }
 return {17,"Inconclusivo: DLL, MCM, sons ou texturas isoladamente nao comprovam a funcao. Se quiser, defina a categoria em regras.tsv."};
}
const vector<string> groups={"Base e interface","Visual e personagens","Gameplay e equipamentos","Missoes e cidades","Animacao e movimento","Audio","Traducoes e dublagens","Gerados e revisao"};
inline int broad(int c){static const int map[]={0,1,1,3,3,2,2,2,2,4,4,5,0,1,6,6,7,7};return map[c];}
struct PriorityRule{string before,after,reason;};
using PriorityRules=vector<PriorityRule>;
inline PriorityRules parsePriorities(const string&data){PriorityRules rules;for(auto&l:lines(data)){if(l[0]=='#')continue;auto a=l.find('\t'),b=a==string::npos?string::npos:l.find('\t',a+1);if(a==string::npos)throw std::runtime_error("prioridades.tsv: use ANTES, TAB, DEPOIS, TAB, motivo.");string before=l.substr(0,a),after=l.substr(a+1,b==string::npos?string::npos:b-a-1);if(!safeName(before)||!safeName(after)||before==after)throw std::runtime_error("Regra de prioridade invalida.");rules.push_back({before,after,b==string::npos?"Regra explicita do usuario":l.substr(b+1)});}return rules;}
inline bool localizedName(const string&name){return hasAny(low(name),{"pt-br","ptbr","pt br","traduc","traduç","traduz","portugues","português","translation","translated","dublag","dubbing","dubbed","traducción","traduccion","español","espanol","castellano","doblaje","subtitles","subtítulos","subtitulos"});}
inline string canonicalModName(string s){s=low(s);for(char&c:s)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||(unsigned char)c>=128))c=' ';std::istringstream in(s);string word,result;const std::set<string>ignored={"pt","br","ptbr","se","sse","ae","traducao","tradução","traducoes","traduções","translation","translated","portugues","português","portuguese","brasil","brazil","brazilian","dublagem","dubbing","dubbed","em","english","spanish","espanol","español","castellano","traduccion","traducción","doblaje","subtitulos","subtítulos","subtitles"};while(in>>word){if(ignored.count(word))continue;bool digits=word.find_first_not_of("0123456789")==string::npos;if(digits||(word.size()>1&&word[0]=='v'&&word.substr(1).find_first_not_of("0123456789")==string::npos))continue;if(!result.empty())result+=' ';result+=word;}return result;}
inline vector<int> priorityOrder(const vector<Mod>&mods,const vector<vector<string>>&files,const std::map<string,vector<int>>&owners,const PriorityRules&explicitRules,vector<string>&notes){
 int n=mods.size();std::map<string,int>index;for(int i=0;i<n;i++)index[mods[i].name]=i;std::map<std::pair<int,int>,string>required;
 for(auto&r:explicitRules){auto a=index.find(r.before),b=index.find(r.after);if(a==index.end()||b==index.end()){notes.push_back("Regra nao usada (mod ausente): "+r.before+" -> "+r.after);continue;}if(mods[a->second].state!='+'||mods[b->second].state!='+'){notes.push_back("Regra nao aplicada a mod desativado: "+r.before+" -> "+r.after);continue;}required[{a->second,b->second}]="REGRA EXPLICITA: "+r.reason;}
 for(int target=0;target<n;target++)if(mods[target].state=='+'&&localizedName(mods[target].name)){
  auto key=canonicalModName(mods[target].name);if(key.size()<6)continue;vector<int>matches;
  for(int source=0;source<n;source++)if(source!=target&&mods[source].state=='+'&&!localizedName(mods[source].name)&&canonicalModName(mods[source].name)==key){
   std::set<string>paths(files[source].begin(),files[source].end());bool overlap=false;for(auto&f:files[target])if(paths.count(f)&&(ends(f,".esp")||ends(f,".esm")||ends(f,".esl")||ends(f,".fuz")||ends(f,".wav")||ends(f,".xwm")||ends(f,".strings")||ends(f,".dlstrings")||ends(f,".ilstrings"))){overlap=true;break;}if(overlap)matches.push_back(source);
  }
  if(matches.size()==1){int source=matches[0];if(!required.count({target,source})&&!required.count({source,target}))required[{source,target}]="INFERIDA: traducao/dublagem com nome-base unico e arquivo de plugin, voz ou texto compartilhado; confira as versoes";}
  else if(matches.size()>1)notes.push_back("REVISAR: mais de um original possivel para "+mods[target].name);
 }
 vector<std::set<int>>edges(n);vector<int>degree(n);auto edge=[&](int a,int b){if(edges[a].insert(b).second)degree[b]++;};
 // Protect every pre-existing active-file relationship unless an explicit or
 // matched-localization rule intentionally changes that exact pair.
 for(auto&[file,list]:owners)for(size_t a=0;a<list.size();a++)for(size_t b=a+1;b<list.size();b++)if(!required.count({list[b],list[a]}))edge(list[a],list[b]);
 for(auto&[pair,reason]:required){edge(pair.first,pair.second);notes.push_back((pair.first>pair.second?"MUDANCA PROPOSTA: ":"JA RESPEITADA: ")+mods[pair.first].name+" -> "+mods[pair.second].name+" | "+reason);}
 vector<int>order;vector<bool>used(n);for(int count=0;count<n;count++){int next=-1;for(int i=0;i<n;i++)if(!used[i]&&degree[i]==0){next=i;break;}if(next<0){string message="Regras de prioridade em ciclo ou incompatíveis com conflitos existentes. Nenhuma ordem sera aplicada. Revise prioridades.tsv e as traducoes. Mods envolvidos:";int shown=0;for(int i=0;i<n&&shown<12;i++)if(!used[i]){message+="\r\n- "+mods[i].name;shown++;}throw std::runtime_error(message);}used[next]=true;order.push_back(next);for(int j:edges[next])degree[j]--;}
 return order;
}
struct Conflict{string file,winner,previousWinner;vector<string>losers;};
inline bool animationOutput(const string&name){auto n=low(name);for(char&c:n)if(c=='_'||c=='-'||c=='.')c=' ';return hasAny(n,{"nemesis","pandora"})&&hasAny(n,{"output","saida","saída","gerados"});}
inline int organizationTier(const Mod&m){if(animationOutput(m.name))return 2;return m.cat==14||m.cat==15||localizedName(m.name)?1:0;}
struct Plan{Parsed p;vector<int>order;vector<string>output,separators,warnings,reasons,online,priorityNotes;vector<Conflict>conflicts;int shared=0,unknown=0,files=0,automatic=0,moved=0;bool prioritiesEnabled=false,localizationLast=false;string serialized;};
// Compact display groups; keep old rule category IDs compatible.
const vector<string> compactGroups={"Requisitos e base","Texturas","Graficos","Gameplay, magias, armas e armaduras","Quests","Interface","Dublagens","Legendas","Prioridades finais","INCONCLUSIVOS - definir categoria"};
inline int separatorCategory(const Mod&m){
 if(animationOutput(m.name))return 8;
 if(m.cat==15)return 6;
 if(m.cat==14||localizedName(m.name))return 7;
 switch(m.cat){
  case 0:case 18:return 0;
  case 1:return 1;
  case 2:case 13:return 2;
  case 3:case 4:return 4;
  case 12:return 5;
  case 16:return 8;
  case 17:return 9;
  default:return 3;
 }
}
inline int compactPriority(const Mod&m){return animationOutput(m.name)?10:separatorCategory(m);}
inline void buildBlocks(Plan&p){
 p.output=p.p.official;p.separators.clear();int number=0;
 for(size_t a=0;a<p.p.mods.size();){
  int category=separatorCategory(p.p.mods[a]);size_t b=a+1;
  while(b<p.p.mods.size()&&separatorCategory(p.p.mods[b])==category)++b;
  string title=tr(compactGroups.at(category));
  string sep="ATMOR - "+std::to_string(++number)+" - "+title+"_separator";
  p.separators.push_back(sep);p.output.push_back("-"+sep);
  for(size_t i=a;i<b;i++)p.output.push_back(string(1,p.p.mods[i].state)+p.p.mods[i].name);
  a=b;
 }
 p.serialized="";for(auto&s:p.p.comments)p.serialized+=s+"\r\n";
 for(auto it=p.output.rbegin();it!=p.output.rend();++it)p.serialized+=*it+"\r\n";
}
inline Plan organize(const string&data,const fs::path&root,const Rules&rules,bool scan=true,const PriorityRules&priorityRules={},bool applyPriorities=false,bool translationsLast=false){
 Plan p;p.p=parse(data);p.prioritiesEnabled=applyPriorities||translationsLast;p.localizationLast=translationsLast;std::map<string,vector<int>>activeOwners;vector<vector<string>>fileLists;int archives=0;
 for(size_t i=0;i<p.p.mods.size();i++){
  auto&m=p.p.mods[i];p.order.push_back((int)i);vector<string>files;
  if(scan)try{files=scanFiles(root/fs::u8path(m.name));}catch(const std::exception&e){throw std::runtime_error("Mod em analise: "+m.name+"\r\n\r\n"+e.what());}
  auto r=rules.find(m.name);if(r!=rules.end()){m.cat=r->second;p.reasons.push_back("Categoria definida em regras.tsv");}else{auto guess=inferCategory(m.name,files);m.cat=guess.cat;p.reasons.push_back(guess.reason);p.automatic++;}
  if(m.cat==17)p.unknown++;
  fileLists.push_back(files);p.files+=(int)files.size();for(auto&f:files){if(ends(f,".bsa")||ends(f,".ba2"))archives++;if(m.state=='+')activeOwners[f].push_back(i);}
 }
 if(translationsLast&&p.unknown){p.localizationLast=false;p.prioritiesEnabled=false;p.warnings.push_back("ORDEM ORIGINAL PRESERVADA: ha mods sem classificacao. Voce pode aplicar os separadores com backup e manter todas as prioridades.");}
 if(translationsLast&&!p.unknown){
 auto isTranslation=[&](int i){auto&m=p.p.mods[i];return m.cat==14||m.cat==15||localizedName(m.name);};
 std::stable_sort(p.order.begin(),p.order.end(),[&](int a,int b){return compactPriority(p.p.mods[a])<compactPriority(p.p.mods[b]);});
 int activeOutputs=0;for(int i:p.order)if(animationOutput(p.p.mods[i].name)){p.priorityNotes.push_back("PRIORIDADE MAXIMA: "+p.p.mods[i].name);if(p.p.mods[i].state=='+')activeOutputs++;}if(activeOutputs>1)p.warnings.push_back("Mais de um Nemesis/Pandora Output ativo: a ordem relativa entre eles foi preservada. Confira qual saida foi gerada para este perfil; o programa nao combina nem regenera animacoes.");
 p.priorityNotes.push_back("ORDEM: Requisitos e base -> Texturas -> Graficos -> Gameplay, magias, armas e armaduras -> Quests -> Interface -> Dublagens -> Legendas -> Prioridades finais. Ordem relativa preservada dentro de cada grupo; Nemesis/Pandora Output fica por ultimo. Agrupar categorias pode mudar vencedores de conflitos. Nenhum mod e ativado ou desativado.");
 for(int i:p.order)if(isTranslation(i))p.priorityNotes.push_back("PRIORIDADE FINAL: "+p.p.mods[i].name+" | "+p.reasons[i]);
 if(!priorityRules.empty())p.priorityNotes.push_back("prioridades.tsv ignorado neste modo no agrupamento compacto.");
 }else if(applyPriorities)p.order=priorityOrder(p.p.mods,fileLists,activeOwners,priorityRules,p.priorityNotes);
 vector<int>rank(p.p.mods.size());for(size_t i=0;i<p.order.size();i++){rank[p.order[i]]=i;if((int)i!=p.order[i])p.moved++;}
 for(auto&[file,owners]:activeOwners)if(owners.size()>1){string previousWinner=p.p.mods[owners.back()].name;std::sort(owners.begin(),owners.end(),[&](int a,int b){return rank[a]<rank[b];});Conflict c;c.file=file;c.previousWinner=previousWinner;c.winner=p.p.mods[owners.back()].name;for(size_t j=0;j+1<owners.size();j++)c.losers.push_back(p.p.mods[owners[j]].name);p.conflicts.push_back(std::move(c));p.shared+=(int)owners.size()-1;}
 auto originalMods=p.p.mods;auto originalReasons=p.reasons;for(size_t i=0;i<p.order.size();i++){p.p.mods[i]=originalMods[p.order[i]];p.reasons[i]=originalReasons[p.order[i]];}
 if(!scan)p.warnings.push_back("PREVIA SEM PASTA MODS: arquivos nao lidos; aplicacao bloqueada.");
 if(archives)p.warnings.push_back(std::to_string(archives)+" arquivos BSA/BA2: conteudo interno nao analisado.");
 p.warnings.push_back(p.moved?"PRIORIDADES REVISADAS: confira as mudancas propostas e suas evidencias. Nenhum mod foi ativado/desativado.":"ORDEM DOS MODS MANTIDA: nenhum deslocamento necessario. Numeros da coluna podem mudar com os separadores.");
 p.warnings.push_back("Categorias automaticas sao inferencias, nao comprovam compatibilidade. O vencedor atual de um arquivo nao significa que seja o vencedor recomendado.");
 if(p.unknown)p.warnings.push_back(std::to_string(p.unknown)+" mods sem categoria conclusiva; nenhuma categoria foi presumida. Confira os itens INCONCLUSIVOS abaixo.");
 buildBlocks(p);return p;
}
