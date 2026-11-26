#pragma once
#include "core.hpp"
#include "online.hpp"
inline string qsettingsText(string s){s=trim(s);if(s.rfind("@ByteArray(",0)==0&&s.size()>11&&s.back()==')')s=s.substr(11,s.size()-12);string r;for(size_t i=0;i<s.size();i++){if(s[i]=='\\'&&i+1<s.size()&&(s[i+1]=='\\'||s[i+1]=='"'))r+=s[++i];else r+=s[i];}return r;}
inline std::map<string,string> iniValues(const string&s){std::map<string,string>r;for(auto&l:lines(s)){auto t=trim(l);if(t.empty()||t[0]=='#'||t[0]==';'||t[0]=='[')continue;auto p=t.find('=');if(p!=string::npos)r[low(trim(t.substr(0,p)))]=qsettingsText(t.substr(p+1));}return r;}
inline vector<string> vdfStrings(const string&s){vector<string>v;for(size_t i=0;i<s.size();i++){if(s[i]!='"')continue;string t;for(++i;i<s.size();i++){if(s[i]=='"')break;if(s[i]=='\\'&&i+1<s.size()&&(s[i+1]=='\\'||s[i+1]=='"'))t+=s[++i];else t+=s[i];}v.push_back(t);}return v;}
inline fs::path resolveMOPath(string v,const fs::path&base){auto at=v.find("%BASE_DIR%");if(at!=string::npos)v.replace(at,10,base.u8string());fs::path p=fs::u8path(v);if(p.empty())return base;if(!p.is_absolute())p=base/p;return p.lexically_normal();}
#ifdef _WIN32
inline fs::path environmentPath(const wchar_t*name){wchar_t p[32768];auto n=GetEnvironmentVariableW(name,p,32768);return n&&n<32768?fs::path(p):fs::path();}
inline fs::path registryPath(HKEY hive,const wchar_t*key,const wchar_t*value){wchar_t data[32768];DWORD bytes=sizeof(data);if(RegGetValueW(hive,key,value,RRF_RT_REG_SZ|RRF_RT_REG_EXPAND_SZ,nullptr,data,&bytes)!=ERROR_SUCCESS)return {};return fs::path(data);}
struct FoundGame{fs::path exe;string source;};
struct FoundProfile{fs::path list,mods,game;string name;};
struct Discovery{vector<FoundGame>games;vector<FoundProfile>profiles;vector<string>notes;};
inline Discovery discover(const fs::path&app,const fs::path&selected){
 Discovery out;std::set<string>seenIni,seenGames,seenProfiles;vector<fs::path>inis;
 auto exists=[](const fs::path&p){std::error_code ec;return !p.empty()&&fs::is_regular_file(nativePath(p),ec);};
 auto addGame=[&](fs::path p,string source){if(p.empty())return;if(low(p.filename().u8string())!="skyrimse.exe")p/=L"SkyrimSE.exe";if(exists(p)&&seenGames.insert(low(fs::absolute(p).u8string())).second)out.games.push_back({fs::absolute(p),source});};
 if(!selected.empty())inis.push_back(selected.parent_path().parent_path().parent_path()/L"ModOrganizer.ini");
 inis.push_back(app/L"ModOrganizer.ini");inis.push_back(app.parent_path()/L"ModOrganizer.ini");inis.push_back(fs::current_path()/L"ModOrganizer.ini");
 auto local=environmentPath(L"LOCALAPPDATA");if(!local.empty()){auto instances=local/L"ModOrganizer";std::error_code ec;if(fs::is_directory(instances,ec))try{for(auto&e:directoryItems(instances))if(e.directory&&!e.link)inis.push_back(e.path/L"ModOrganizer.ini");}catch(const std::exception&e){out.notes.push_back(e.what());}}
 for(auto&ini:inis){if(!exists(ini)||!seenIni.insert(low(fs::absolute(ini).u8string())).second)continue;try{auto values=iniValues(readFile(nativePath(ini)));auto game=fs::u8path(values["gamepath"]);if(!game.empty()&&!game.is_absolute())game=ini.parent_path()/game;addGame(game,"MO2: "+ini.parent_path().filename().u8string());
 if(game.empty()||!exists(game/L"SkyrimSE.exe"))continue;
 auto base=values["base_directory"].empty()?ini.parent_path():resolveMOPath(values["base_directory"],ini.parent_path());auto profiles=values["profiles_directory"].empty()?base/L"profiles":resolveMOPath(values["profiles_directory"],base);auto mods=values["mod_directory"].empty()?base/L"mods":resolveMOPath(values["mod_directory"],base);
 auto selectedName=values["selected_profile"];vector<fs::path>lists;if(!selected.empty()&&low(selected.parent_path().parent_path().u8string())==low(profiles.u8string())&&exists(selected))lists.push_back(selected);if(!selectedName.empty()&&safeName(selectedName)&&exists(profiles/fs::u8path(selectedName)/L"modlist.txt"))lists.push_back(profiles/fs::u8path(selectedName)/L"modlist.txt");else {std::error_code ec;if(fs::is_directory(profiles,ec))for(auto&e:directoryItems(profiles))if(e.directory&&!e.link&&exists(e.path/L"modlist.txt"))lists.push_back(e.path/L"modlist.txt");}
 for(auto&p:lists)if(seenProfiles.insert(low(fs::absolute(p).u8string())).second)out.profiles.push_back({p,mods,game/L"SkyrimSE.exe",ini.parent_path().filename().u8string()+" / "+p.parent_path().filename().u8string()});
 }catch(const std::exception&e){out.notes.push_back(e.what());}}
 addGame(registryPath(HKEY_LOCAL_MACHINE,L"SOFTWARE\\WOW6432Node\\Bethesda Softworks\\Skyrim Special Edition",L"Installed Path"),"Registro Bethesda");
 addGame(registryPath(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Bethesda Softworks\\Skyrim Special Edition",L"Installed Path"),"Registro Bethesda");
 vector<fs::path>steamRoots={registryPath(HKEY_CURRENT_USER,L"Software\\Valve\\Steam",L"SteamPath"),registryPath(HKEY_LOCAL_MACHINE,L"SOFTWARE\\WOW6432Node\\Valve\\Steam",L"InstallPath")};
 auto pf=environmentPath(L"ProgramFiles(x86)");if(!pf.empty())steamRoots.push_back(pf/L"Steam");vector<fs::path>libraries=steamRoots;
 for(auto&steam:steamRoots)if(!steam.empty()){auto file=steam/L"steamapps"/L"libraryfolders.vdf";if(exists(file))try{auto tokens=vdfStrings(readFile(nativePath(file)));for(size_t i=0;i+1<tokens.size();i++)if(tokens[i]=="path")libraries.push_back(fs::u8path(tokens[i+1]));}catch(...){}}
 for(auto&lib:libraries)if(!lib.empty()){auto manifest=lib/L"steamapps"/L"appmanifest_489830.acf";string install="Skyrim Special Edition";if(exists(manifest))try{auto tokens=vdfStrings(readFile(nativePath(manifest)));for(size_t i=0;i+1<tokens.size();i++)if(tokens[i]=="installdir"&&safeName(tokens[i+1]))install=tokens[i+1];}catch(...){}addGame(lib/L"steamapps"/L"common"/fs::u8path(install),"Steam");}
 // Bounded registry discovery covers custom GOG install folders without scanning disks.
 HKEY games=nullptr;if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\WOW6432Node\\GOG.com\\Games",0,KEY_READ,&games)==ERROR_SUCCESS){for(DWORD i=0;i<512;i++){wchar_t name[256];DWORD n=256;if(RegEnumKeyExW(games,i,name,&n,nullptr,nullptr,nullptr,nullptr)!=ERROR_SUCCESS)break;addGame(registryPath(games,name,L"path"),"GOG");}RegCloseKey(games);}
 return out;
}
inline vector<fs::path> automaticLogs(const fs::path&profile,const fs::path&mods){vector<fs::path>paths;PWSTR docs=nullptr;if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents,0,nullptr,&docs))){for(auto&name:{L"Skyrim Special Edition",L"Skyrim Special Edition GOG"})paths.push_back(fs::path(docs)/L"My Games"/name/L"SKSE"/L"skse64.log");CoTaskMemFree(docs);}paths.push_back(mods.parent_path()/L"overwrite"/L"SKSE"/L"skse64.log");paths.push_back(profile.parent_path()/L"SKSE"/L"skse64.log");vector<fs::path>found;std::error_code ec;for(auto&p:paths)if(fs::is_regular_file(nativePath(p),ec))found.push_back(p);std::sort(found.begin(),found.end(),[](const fs::path&a,const fs::path&b){std::error_code x,y;auto ta=fs::last_write_time(nativePath(a),x),tb=fs::last_write_time(nativePath(b),y);return !x&&!y&&ta>tb;});return found;}

inline fs::path diagnosticOverwrite(const fs::path&profile,const fs::path&mods){
 auto base=profile.parent_path().parent_path().parent_path();
 for(auto&ini:{base/"ModOrganizer.ini",mods.parent_path()/"ModOrganizer.ini"}){std::error_code ec;if(!fs::is_regular_file(nativePath(ini),ec))continue;
 auto values=iniValues(readFile(nativePath(ini)));auto root=ini.parent_path();
 if(values.count("base_directory"))root=resolveMOPath(values["base_directory"],root);
 if(values.count("overwrite_directory"))return resolveMOPath(values["overwrite_directory"],root);
 return root/"overwrite";
 }return mods.parent_path()/"overwrite";
}
inline vector<fs::path> automaticCrashes(const fs::path&profile,const fs::path&mods){
 vector<fs::path>dirs={diagnosticOverwrite(profile,mods)/"SKSE",profile.parent_path()/"SKSE"};PWSTR docs=nullptr;
 if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents,0,nullptr,&docs))){for(auto name:{L"Skyrim Special Edition",L"Skyrim Special Edition GOG"})dirs.push_back(fs::path(docs)/L"My Games"/name/L"SKSE");CoTaskMemFree(docs);}
 vector<fs::path>found;for(auto dir:dirs)for(auto sub:{fs::path(),fs::path("CrashLogs")}){std::error_code ec;if(!fs::is_directory(nativePath(dir/sub),ec))continue;try{for(auto&e:directoryItems(dir/sub)){auto n=low(e.path.filename().u8string());if(!e.directory&&!e.link&&n.rfind("crash-",0)==0&&ends(n,".log"))found.push_back(e.path);}}catch(...){} }
 std::sort(found.begin(),found.end(),[](auto&a,auto&b){std::error_code x,y;return fs::last_write_time(nativePath(a),x)>fs::last_write_time(nativePath(b),y);});return found;
}
#endif
