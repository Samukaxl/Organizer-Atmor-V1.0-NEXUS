#pragma once
#include "core.hpp"
#include <functional>
#include <optional>
struct ProfileEdit{fs::path path;string before,after;};
struct SeparatorData{string name;std::optional<string>meta;};
struct Cleanup{vector<ProfileEdit>edits;vector<SeparatorData>retired;vector<string>ensure;};
inline vector<string> modEntries(const string&s){vector<string>r;for(auto&l:lines(s))if(l[0]!='#'&&!ends(l,"_separator"))r.push_back(l);return r;}
inline std::optional<string> separatorMetadata(const fs::path&dir){
 if(!fs::is_directory(nativePath(dir))||linked(dir))throw std::runtime_error("Separador invalido: "+dir.u8string());
 std::optional<string>meta;
 for(auto&e:directoryItems(dir)){if(low(e.path.filename().u8string())!="meta.ini"||e.link||e.directory)throw std::runtime_error("Separador contem arquivos adicionais. Para protege-los, a limpeza foi interrompida: "+dir.u8string());meta=readFile(e.path);}
 return meta;
}
inline string stripSeparators(const string&s,const std::set<string>&names){bool found=false;for(auto&l:lines(s))if(l.size()>1&&names.count(low(l.substr(1))))found=true;if(!found)return s;string r;bool bom=s.compare(0,3,"\xef\xbb\xbf")==0;if(bom)r="\xef\xbb\xbf";for(auto&l:lines(s))if(!(l.size()>1&&(l[0]=='+'||l[0]=='-')&&names.count(low(l.substr(1)))))r+=l+"\r\n";return r;}
inline Cleanup planCleanup(const fs::path&root,const fs::path&profile,const Plan&p){
 Cleanup c;c.ensure=p.separators;std::set<string>keep,remove;
 for(auto&n:p.separators)keep.insert(low(n));
 for(auto&e:directoryItems(root)){string n=e.path.filename().u8string();if(!ends(n,"_separator"))continue;if(!safeName(n))throw std::runtime_error("Nome de separador invalido.");auto meta=separatorMetadata(root/fs::u8path(n));if(!keep.count(low(n))){c.retired.push_back({n,meta});remove.insert(low(n));}}
 std::sort(c.retired.begin(),c.retired.end(),[](auto&a,auto&b){return a.name<b.name;});
 auto actual=fs::absolute(profile).lexically_normal();bool found=false;
 for(auto&e:directoryItems(profile.parent_path().parent_path())){if(!e.directory||e.link)continue;auto file=profile.parent_path().parent_path()/e.path.filename()/L"modlist.txt";if(!fs::is_regular_file(nativePath(file)))continue;
 string before=readFile(nativePath(file));bool isCurrent=fs::absolute(file).lexically_normal()==actual;string after=isCurrent?p.serialized:stripSeparators(before,remove);
 auto oldEntries=modEntries(before),newEntries=modEntries(after);if(isCurrent&&p.prioritiesEnabled){std::sort(oldEntries.begin(),oldEntries.end());std::sort(newEntries.begin(),newEntries.end());}if(oldEntries!=newEntries)throw std::runtime_error("Verificacao falhou: mods ou estados seriam alterados indevidamente.");
 if(isCurrent||after!=before)c.edits.push_back({file,before,after});if(isCurrent)found=true;
 }
 if(!found)throw std::runtime_error("Perfil atual nao encontrado entre os perfis. Confira o caminho.");
 std::sort(c.edits.begin(),c.edits.end(),[](auto&a,auto&b){return a.path.u8string()<b.path.u8string();});return c;
}
inline string encodeHex(const string&s){const char*h="0123456789abcdef";string r;for(unsigned char c:s){r+=h[c>>4];r+=h[c&15];}return r;}
inline string decodeHex(const string&s){if(s.size()%2)throw std::runtime_error("Manifesto de backup invalido.");string r;for(size_t i=0;i<s.size();i+=2){auto d=[](char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;throw std::runtime_error("Manifesto invalido.");};r+=(char)(d(s[i])*16+d(s[i+1]));}return r;}
inline void replaceAtomic(const fs::path&path,const string&data){
 auto p=nativePath(path),tmp=p;tmp+=L".samukxll-tmp";if(fs::exists(tmp))throw std::runtime_error("Temporario existente. Confira: "+tmp.u8string());writeFile(tmp,data);
#ifdef _WIN32
 if(!MoveFileExW(tmp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){auto code=GetLastError();fs::remove(tmp);throw std::runtime_error(winError("Falha ao substituir a lista.",path,code));}
#else
 try{fs::rename(tmp,p);}catch(...){fs::remove(tmp);throw;}
#endif
}
inline void makeSeparator(const fs::path&root,const SeparatorData&data){auto dir=nativePath(root/fs::u8path(data.name));if(fs::exists(dir)){if(separatorMetadata(dir)!=data.meta)throw std::runtime_error("Separador com conteudo diferente do backup: "+data.name);return;}fs::create_directory(dir);if(data.meta)writeFile(dir/L"meta.ini",*data.meta);}
inline void removeSeparator(const fs::path&root,const SeparatorData&data){auto dir=nativePath(root/fs::u8path(data.name));if(separatorMetadata(dir)!=data.meta)throw std::runtime_error("Separador alterado desde a analise: "+data.name);if(data.meta&&!fs::remove(dir/L"meta.ini"))throw std::runtime_error("Falha ao retirar meta.ini.");if(!fs::remove(dir))throw std::runtime_error("Falha ao retirar separador vazio: "+data.name);}
// Every modified profile and separator is saved before the first mutation.
// checkpoint is used only by regression tests to inject failures.
inline void executeTransaction(const fs::path&root,const fs::path&profile,const fs::path&backup,
 const vector<ProfileEdit>&edits,const vector<SeparatorData>&retired,const vector<SeparatorData>&ensure,
 std::function<void(int)>checkpoint={}){
 for(auto&e:edits)if(readFile(nativePath(e.path))!=e.before)throw std::runtime_error("Perfil alterado. Analise novamente: "+e.path.u8string());
 for(auto&s:retired)if(separatorMetadata(root/fs::u8path(s.name))!=s.meta)throw std::runtime_error("Separador alterado. Analise novamente.");
 vector<SeparatorData>created;
 for(auto&s:ensure){auto dir=nativePath(root/fs::u8path(s.name));if(!fs::exists(dir))created.push_back(s);else separatorMetadata(dir);}
 fs::create_directories(nativePath(backup/L"perfis"));fs::create_directories(nativePath(backup/L"retirados"));
 writeFile(nativePath(backup/L"formato.txt"),"4");writeFile(nativePath(backup/L"perfil.txt"),fs::absolute(profile).u8string());writeFile(nativePath(backup/L"mods.txt"),fs::absolute(root).u8string());
 string manifest;for(size_t i=0;i<edits.size();i++){auto name=std::to_string(i);writeFile(nativePath(backup/L"perfis"/fs::u8path(name+".txt")),edits[i].before);manifest+=name+"\t"+encodeHex(fs::absolute(edits[i].path).u8string())+"\r\n";if(fs::absolute(edits[i].path)==fs::absolute(profile)){writeFile(nativePath(backup/L"original-modlist.txt"),edits[i].before);writeFile(nativePath(backup/L"aplicado-modlist.txt"),edits[i].after);}}
 writeFile(nativePath(backup/L"perfis.tsv"),manifest);manifest="";
 for(auto&s:retired){makeSeparator(backup/L"retirados",s);manifest+=s.name+"\r\n";}writeFile(nativePath(backup/L"retirados.txt"),manifest);manifest="";for(auto&s:created)manifest+=s.name+"\r\n";writeFile(nativePath(backup/L"criados.txt"),manifest);writeFile(nativePath(backup/L"estado.txt"),"BACKUP_PRONTO");
 vector<SeparatorData>made,removed;vector<size_t>written;
 try{
  for(auto&s:created){made.push_back(s);makeSeparator(root,s);}if(checkpoint)checkpoint(1);
  for(auto&s:retired){removed.push_back(s);removeSeparator(root,s);}if(checkpoint)checkpoint(2);
  for(size_t i=0;i<edits.size();i++){if(readFile(nativePath(edits[i].path))!=edits[i].before)throw std::runtime_error("Perfil mudou durante aplicacao.");replaceAtomic(edits[i].path,edits[i].after);written.push_back(i);if(checkpoint)checkpoint(3);}
  writeFile(nativePath(backup/L"estado.txt"),"CONCLUIDO");
 }catch(const std::exception&error){string failures;
  for(auto it=written.rbegin();it!=written.rend();++it)try{replaceAtomic(edits[*it].path,edits[*it].before);}catch(const std::exception&e){failures+="\r\n"+string(e.what());}
  for(auto&s:removed)try{auto dir=nativePath(root/fs::u8path(s.name));if(fs::exists(dir)&&!fs::exists(dir/L"meta.ini")&&s.meta){writeFile(dir/L"meta.ini",*s.meta);}else makeSeparator(root,s);}catch(const std::exception&e){failures+="\r\n"+string(e.what());}
  for(auto&s:made)try{auto dir=nativePath(root/fs::u8path(s.name));if(fs::exists(dir)){auto meta=separatorMetadata(dir);if(meta&&meta!=s.meta)throw std::runtime_error("Novo separador modificado: "+s.name);removeSeparator(root,{s.name,meta});}}catch(const std::exception&e){failures+="\r\n"+string(e.what());}
  writeFile(nativePath(backup/L"estado.txt"),failures.empty()?"REVERTIDO":"RECUPERACAO_NECESSARIA");
  throw std::runtime_error(string(error.what())+(failures.empty()?"\r\nA operacao foi revertida.":"\r\nRecuperacao incompleta. Backup: "+backup.u8string()+failures));
 }
}
inline void restoreV4(const fs::path&backup,const fs::path&safety){
 auto root=fs::u8path(readFile(nativePath(backup/L"mods.txt"))),profile=fs::u8path(readFile(nativePath(backup/L"perfil.txt")));
 if(!root.is_absolute()||!profile.is_absolute()||profile.filename()!=L"modlist.txt")throw std::runtime_error("Caminhos invalidos no backup.");
 vector<ProfileEdit>edits;std::map<string,string>targets;
 for(auto&line:lines(readFile(nativePath(backup/L"perfis.tsv")))){auto tab=line.find('\t');if(tab==string::npos)throw std::runtime_error("Manifesto invalido.");auto idx=line.substr(0,tab);if(idx.empty()||idx.find_first_not_of("0123456789")!=string::npos)throw std::runtime_error("Indice invalido.");auto path=fs::u8path(decodeHex(line.substr(tab+1)));if(!path.is_absolute()||path.filename()!=L"modlist.txt")throw std::runtime_error("Perfil invalido no backup.");auto original=readFile(nativePath(backup/L"perfis"/fs::u8path(idx+".txt")));edits.push_back({path,readFile(nativePath(path)),original});targets[fs::absolute(path).lexically_normal().u8string()]=original;}
 vector<SeparatorData>ensure,retired;
 for(auto&n:lines(readFile(nativePath(backup/L"retirados.txt")))){if(!safeName(n)||!ends(n,"_separator"))throw std::runtime_error("Nome invalido no backup.");auto data=separatorMetadata(backup/L"retirados"/fs::u8path(n));auto target=root/fs::u8path(n);if(fs::exists(nativePath(target))&&separatorMetadata(target)!=data)throw std::runtime_error("Separador foi recriado com dados diferentes: "+n);ensure.push_back({n,data});}
 for(auto&n:lines(readFile(nativePath(backup/L"criados.txt")))){if(!safeName(n)||!ends(n,"_separator"))throw std::runtime_error("Nome de separador invalido.");auto dir=root/fs::u8path(n);if(!fs::exists(nativePath(dir)))continue;bool used=false;
  for(auto&e:directoryItems(profile.parent_path().parent_path())){if(!e.directory)continue;auto f=profile.parent_path().parent_path()/e.path.filename()/L"modlist.txt";if(!fs::is_regular_file(nativePath(f)))continue;auto t=targets.find(fs::absolute(f).lexically_normal().u8string());auto contents=t==targets.end()?readFile(nativePath(f)):t->second;for(auto&l:lines(contents))if(l.size()>1&&low(l.substr(1))==low(n))used=true;}
  if(!used)retired.push_back({n,separatorMetadata(dir)});
 }
 executeTransaction(root,profile,safety,edits,retired,ensure);
}
