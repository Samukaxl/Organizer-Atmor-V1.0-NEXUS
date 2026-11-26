#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <memory>
#include <tlhelp32.h>
#include <thread>
#include "i18n.hpp"
#include "core.hpp"
#include "transaction.hpp"
#include "online.hpp"
#include "diagnostics.hpp"
#include "full-diagnostics.hpp"
#include "discovery.hpp"
void loadLanguage(){DWORD value=0,size=sizeof(value);if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\OrganizerAtmor",L"Language",RRF_RT_REG_DWORD,nullptr,&value,&size)==ERROR_SUCCESS&&value<3)atmor::language=(int)value;}
void saveLanguage(){HKEY key;if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\OrganizerAtmor",0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)==ERROR_SUCCESS){DWORD v=atmor::language;RegSetValueExW(key,L"Language",0,REG_DWORD,(const BYTE*)&v,sizeof(v));RegCloseKey(key);}}
void changeLanguage(int language);
bool advanced=false;
bool discoveryPending=false;
HWND win,profileBox,modsBox,report,statusBox,applyButton;HFONT font;fs::path appDir;Plan current;Cleanup currentCleanup;string snapshot,ruleSnapshot;bool prioritiesRequested=false;fs::path selectedProfile,selectedMods;bool ready=false,busy=false;
std::wstring wide(const string&s){if(s.empty())return L"";int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),s.size(),0,0);if(!n)throw std::runtime_error("Texto UTF-8 invalido.");std::wstring w(n,L' ');MultiByteToWideChar(CP_UTF8,0,s.data(),s.size(),w.data(),n);return w;}
string utf8(const std::wstring&w){int n=WideCharToMultiByte(CP_UTF8,0,w.data(),w.size(),0,0,0,0);string s(n,' ');WideCharToMultiByte(CP_UTF8,0,w.data(),w.size(),s.data(),n,0,0);return s;}
std::wstring value(HWND h){int n=GetWindowTextLengthW(h);std::wstring w(n+1,0);GetWindowTextW(h,w.data(),n+1);w.resize(n);if(w.size()>1&&w.front()==L'"'&&w.back()==L'"')w=w.substr(1,w.size()-2);return w;}
void text(HWND h,const string&s){SetWindowTextW(h,wide(s).c_str());}
void copyReport(){
 auto content=value(report);SIZE_T bytes=(content.size()+1)*sizeof(wchar_t);
 HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,bytes);if(!memory)throw std::runtime_error(tr("Sem memoria para copiar o relatorio."));
 void* dest=GlobalLock(memory);if(!dest){GlobalFree(memory);throw std::runtime_error(tr("Falha ao preparar copia."));}
 memcpy(dest,content.c_str(),bytes);GlobalUnlock(memory);
 if(!OpenClipboard(win)){GlobalFree(memory);throw std::runtime_error(tr("Area de transferencia ocupada. Tente novamente."));}
 EmptyClipboard();if(!SetClipboardData(CF_UNICODETEXT,memory)){CloseClipboard();GlobalFree(memory);throw std::runtime_error(tr("Nao foi possivel copiar o relatorio."));}CloseClipboard();
 text(statusBox,tr("Relatório copiado. Ele pode conter caminhos pessoais; revise antes de publicar."));
}
void alert(const string&s){MessageBoxW(win,wide(s).c_str(),L"Organizer Atmor",MB_OK|MB_ICONINFORMATION);}
bool moRunning(){HANDLE h=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(h==INVALID_HANDLE_VALUE)throw std::runtime_error(tr("Nao foi possivel verificar se o MO2 esta fechado."));PROCESSENTRY32W p{};p.dwSize=sizeof(p);bool found=false;if(Process32FirstW(h,&p))do{auto s=low(utf8(p.szExeFile));if(s=="modorganizer.exe"||s=="modorganizer2.exe"){found=true;break;}}while(Process32NextW(h,&p));else{CloseHandle(h);throw std::runtime_error(tr("Falha ao consultar processos."));}CloseHandle(h);return found;}
void requireClosed(){if(moRunning())throw std::runtime_error(tr("Feche o Mod Organizer 2 antes de aplicar ou restaurar. Se abriu pelo MO2, desbloqueie a janela e feche o MO2; mantenha esta ferramenta aberta."));}
fs::path chooseFile(bool backup=false){wchar_t buf[32768]={0};OPENFILENAMEW o{};o.lStructSize=sizeof(o);o.hwndOwner=win;o.lpstrFile=buf;o.nMaxFile=32768;o.lpstrFilter=atmor::language==1?L"Mod list (*.txt)\0*.txt\0All files\0*.*\0":atmor::language==2?L"Lista de mods (*.txt)\0*.txt\0Todos los archivos\0*.*\0":L"Lista de mods (*.txt)\0*.txt\0Todos os arquivos\0*.*\0";o.lpstrTitle=backup?tw("Escolha original-modlist.txt dentro de um backup"):tw("Escolha modlist.txt do perfil ativo");o.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;return GetOpenFileNameW(&o)?fs::path(buf):fs::path();}
// Modern Explorer-style folder picker: address bar, navigation, search and resizing.
fs::path chooseFolder(){
 IFileOpenDialog* raw=nullptr;
 HRESULT hr=CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&raw));
 if(FAILED(hr))throw std::runtime_error(tr("Nao foi possivel abrir o seletor do Windows. Cole o caminho diretamente no campo Pasta mods."));
 auto release=[](IFileOpenDialog*p){p->Release();};
 std::unique_ptr<IFileOpenDialog,decltype(release)> dlg(raw,release);
 FILEOPENDIALOGOPTIONS options{};
 hr=dlg->GetOptions(&options);
 if(SUCCEEDED(hr))hr=dlg->SetOptions(options|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST|FOS_NOCHANGEDIR);
 if(FAILED(hr))throw std::runtime_error(tr("Falha ao preparar a selecao de pasta."));
 dlg->SetTitle(tw("Selecione a pasta mods do Mod Organizer 2"));
 dlg->SetOkButtonLabel(tw("Selecionar esta pasta"));
 // A separate client identity lets Windows remember this dialog's own size/location.
 const GUID pickerId={0xe2fa013a,0xb54a,0x4789,{0xb9,0xa9,0x43,0x55,0xb2,0x08,0x8d,0x90}};
 dlg->SetClientGuid(pickerId);
 fs::path initial(value(modsBox));std::error_code ec;
 if(initial.empty()||!fs::is_directory(initial,ec)){
   auto profile=fs::path(value(profileBox));
   auto candidate=profile.parent_path().parent_path().parent_path()/L"mods";
   initial=fs::is_directory(candidate,ec)?candidate:profile.parent_path();
 }
 if(!initial.empty()&&fs::is_directory(initial,ec)){
   IShellItem* item=nullptr;
   if(SUCCEEDED(SHCreateItemFromParsingName(fs::absolute(initial).c_str(),nullptr,IID_PPV_ARGS(&item)))){
     dlg->SetFolder(item);item->Release();
   }
 }
 hr=dlg->Show(win);
 if(hr==HRESULT_FROM_WIN32(ERROR_CANCELLED))return {};
 if(FAILED(hr))throw std::runtime_error(tr("Falha ao abrir a pasta. Voce tambem pode colar seu caminho no campo Pasta mods."));
 IShellItem* item=nullptr;hr=dlg->GetResult(&item);
 if(FAILED(hr))throw std::runtime_error(tr("Nao foi possivel obter a pasta selecionada."));
 PWSTR path=nullptr;hr=item->GetDisplayName(SIGDN_FILESYSPATH,&path);item->Release();
 if(FAILED(hr))throw std::runtime_error(tr("Escolha uma pasta local ou de rede acessivel."));
 fs::path result(path);CoTaskMemFree(path);return result;
}
fs::path gameExe, importedLog;
string gameVersion;
fs::path selectDiagnosticFile(bool log=false){wchar_t buf[32768]={0};OPENFILENAMEW o{};o.lStructSize=sizeof(o);o.hwndOwner=win;o.lpstrFile=buf;o.nMaxFile=32768;o.lpstrFilter=log?L"Logs (*.log;*.txt)\0*.log;*.txt\0":L"SkyrimSE.exe\0SkyrimSE.exe\0";o.lpstrTitle=log?tw("Selecione o crash ou log SKSE da ultima tentativa"):tw("Selecione SkyrimSE.exe da instalacao usada no MO2");o.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;return GetOpenFileNameW(&o)?fs::path(buf):fs::path();}
string executableVersion(const fs::path&p){DWORD unused=0;auto file=nativePath(p);DWORD n=GetFileVersionInfoSizeW(file.c_str(),&unused);if(!n||n>1024*1024)throw std::runtime_error(tr("Nao foi possivel ler a versao do executavel selecionado."));vector<unsigned char> buffer(n);if(!GetFileVersionInfoW(file.c_str(),0,n,buffer.data()))throw std::runtime_error(tr("Falha ao ler versao do Skyrim."));VS_FIXEDFILEINFO*info=nullptr;UINT size=0;if(!VerQueryValueW(buffer.data(),L"\\",(void**)&info,&size)||size<sizeof(*info)||info->dwSignature!=0xfeef04bd)throw std::runtime_error(tr("Recurso de versao invalido."));return std::to_string(HIWORD(info->dwFileVersionMS))+"."+std::to_string(LOWORD(info->dwFileVersionMS))+"."+std::to_string(HIWORD(info->dwFileVersionLS))+"."+std::to_string(LOWORD(info->dwFileVersionLS));}
void setGame(const fs::path&p){if(low(p.filename().u8string())!="skyrimse.exe")throw std::runtime_error(tr("Esta verificacao e para Skyrim Special Edition. Selecione SkyrimSE.exe."));auto v=executableVersion(p);gameExe=p;gameVersion=v;text(GetDlgItem(win,208),"Skyrim "+v+" | "+p.u8string());}
void detectGame(){if(!gameExe.empty())return;throw std::runtime_error(tr("Aguarde a deteccao automatica ou escolha uma instalacao na lista Skyrim. Se nao aparecer, use Localizar exe."));}
struct DiagnosticResult{Diagnostics data;string error;};
Discovery detected;bool updatingDiscovery=false;
void layout();
void beginDiscovery();
void applyDiscovery(Discovery d);
void runDiagnostics();
void setBusy(bool b){busy=b;for(int id:{101,102,103,104,105,106,107,108,109,113,114,115,116,117,118,119,120,121,122,123})EnableWindow(GetDlgItem(win,id),!b);EnableWindow(applyButton,!b&&ready);}
void runDiagnostics(){
 auto p=fs::path(value(profileBox)),m=fs::path(value(modsBox));if(p.empty()||m.empty())throw std::runtime_error(tr("Selecione o perfil e a pasta mods primeiro."));
 detectGame();
 // Re-read version on every run; the game may have been updated since selection.
 setGame(gameExe);auto exe=gameExe,log=importedLog;auto version=gameVersion;
 setBusy(true);text(statusBox,tr("Conferindo runtime, dependencias dos plugins e DLLs SKSE..."));
 std::thread([p,m,exe,log,version](){auto*r=new DiagnosticResult;try{string logs;fs::path logPath=log;bool autoLog=log.empty();if(autoLog){auto candidates=automaticLogs(p,m);if(!candidates.empty())logPath=candidates.front();}string logInfo;
 if(!logPath.empty()){std::error_code ec;if(fs::file_size(nativePath(logPath),ec)>8*1024*1024||ec)throw std::runtime_error(tr("Log inacessivel ou maior que 8 MiB."));logs=readFile(nativePath(logPath));WIN32_FILE_ATTRIBUTE_DATA attr{};if(GetFileAttributesExW(nativePath(logPath).c_str(),GetFileExInfoStandard,&attr)){FILETIME localTime;SYSTEMTIME time{};FileTimeToLocalFileTime(&attr.ftLastWriteTime,&localTime);FileTimeToSystemTime(&localTime,&time);char formatted[64];snprintf(formatted,sizeof(formatted),"%04d-%02d-%02d %02d:%02d",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute);logInfo=formatted;}}
 string installedSkse;try{installedSkse=executableVersion(exe.parent_path()/L"skse64_loader.exe");}catch(...){}auto overwrite=diagnosticOverwrite(p,m);auto d=diagnose(p,m,exe.parent_path()/L"Data",version,logs,installedSkse,overwrite);d.exe=exe.u8string();d.rows.push_back({"ORIGEM DO LOG","SKSE",logPath.empty()?"Nenhum log foi encontrado nos caminhos conhecidos. Voce pode usar Importar log SKSE.":string(autoLog?"Log encontrado automaticamente: ":"Log escolhido: ")+logPath.u8string()+"\r\nUltima gravacao (hora local): "+logInfo+". A associacao com este perfil nao e comprovada; confira a data e execute SKSE pelo perfil correto.","",""});
 auto loader=exe.parent_path()/L"skse64_loader.exe";std::error_code ec;if(fs::exists(loader,ec)){try{d.rows.push_back({"VERSAO INSTALADA","SKSE loader","Versao do recurso de skse64_loader.exe: "+executableVersion(loader)+". Compare com a referencia do runtime; nao basta trocar apenas o loader.","https://skse.silverlock.org/","Conferir pacote SKSE"});}catch(...) {}}
 string crash;fs::path crashPath;
 if(!logs.empty()&&hasAny(low(logs),{"crashlogger","unhandled exception"})){crash=logs;crashPath=logPath;}
 else{auto found=automaticCrashes(p,m);if(!found.empty()){crashPath=found.front();std::error_code ce;auto size=fs::file_size(nativePath(crashPath),ce);if(!ce&&size<=8*1024*1024)crash=readFile(nativePath(crashPath));else d.rows.push_back({"LEITURA INCOMPLETA","Crash log","Arquivo inacessivel ou acima de 8 MiB: "+crashPath.u8string(),"",""});}}
 completeDiagnostics(d,p,m,exe.parent_path()/L"Data",overwrite,crash,crashPath.u8string());
 if(!logs.empty())d.rows.push_back({"LOG ORIGINAL",logPath.u8string(),logs,"",""});
 if(!crash.empty()&&crashPath!=logPath)d.rows.push_back({"LOG ORIGINAL",crashPath.u8string(),crash,"",""});
 r->data=std::move(d);}catch(const std::exception&e){r->error=e.what();}PostMessageW(win,WM_APP+3,0,(LPARAM)r);}).detach();
}
// Ignore delayed WM_TIMER messages after analysis has consumed the selected paths.
void cancelPendingDiscovery(){discoveryPending=false;KillTimer(win,1);}
void setPathIfChanged(HWND field,const fs::path& path){
 if(value(field)!=path.wstring())SetWindowTextW(field,path.c_str());
}
void chooseDetectedProfile(int index){if(index<0||index>=(int)detected.profiles.size())return;auto&p=detected.profiles[index];cancelPendingDiscovery();updatingDiscovery=true;setPathIfChanged(profileBox,p.list);setPathIfChanged(modsBox,p.mods);updatingDiscovery=false;setGame(p.game);for(size_t i=0;i<detected.games.size();i++)if(low(detected.games[i].exe.u8string())==low(p.game.u8string()))SendMessageW(GetDlgItem(win,120),CB_SETCURSEL,i,0);importedLog.clear();}
void beginDiscovery(){if(busy)return;cancelPendingDiscovery();auto p=fs::path(value(profileBox));setBusy(true);text(statusBox,tr("Detectando perfil MO2, instalacoes Steam/GOG e versao do Skyrim..."));std::thread([p](){auto*d=new Discovery;try{*d=discover(appDir,p);}catch(const std::exception&e){d->notes.push_back(e.what());}PostMessageW(win,WM_APP+4,0,(LPARAM)d);}).detach();}
void applyDiscovery(Discovery d){detected=std::move(d);auto games=GetDlgItem(win,120),profiles=GetDlgItem(win,121);SendMessageW(games,CB_RESETCONTENT,0,0);SendMessageW(profiles,CB_RESETCONTENT,0,0);int gameIndex=-1,profileIndex=-1;auto selected=low(utf8(value(profileBox)));
 for(size_t i=0;i<detected.games.size();i++){auto&g=detected.games[i];string label=g.source+" | ";try{label+="Skyrim "+executableVersion(g.exe);}catch(...){label+=tr("versao ilegivel");}label+=" | "+g.exe.u8string();SendMessageW(games,CB_ADDSTRING,0,(LPARAM)wide(label).c_str());if(!gameExe.empty()&&low(g.exe.u8string())==low(gameExe.u8string()))gameIndex=(int)i;}
 for(size_t i=0;i<detected.profiles.size();i++){auto&p=detected.profiles[i];SendMessageW(profiles,CB_ADDSTRING,0,(LPARAM)wide(p.name).c_str());if(low(p.list.u8string())==selected)profileIndex=(int)i;}
 if(selected.empty()&&detected.profiles.size()==1)profileIndex=0;
 if(profileIndex>=0){SendMessageW(profiles,CB_SETCURSEL,profileIndex,0);chooseDetectedProfile(profileIndex);}else if(gameIndex>=0){SendMessageW(games,CB_SETCURSEL,gameIndex,0);setGame(detected.games[gameIndex].exe);}else if(detected.games.size()==1){SendMessageW(games,CB_SETCURSEL,0,0);setGame(detected.games[0].exe);}else {gameExe.clear();gameVersion.clear();text(GetDlgItem(win,208),detected.games.empty()?tr("Skyrim não encontrado. Use Localizar exe para indicar uma instalação fora dos caminhos conhecidos."):tr("Mais de uma instalação: escolha o Skyrim correto na lista acima."));}
 setBusy(false);if(value(profileBox).empty()||value(modsBox).empty()){advanced=true;layout();}
 if(ready)text(statusBox,tr("Previa preservada. Feche o MO2 e clique em 2. Aplicar com backup."));else text(statusBox,std::to_string(detected.profiles.size())+tr(" perfil(is) e ")+std::to_string(detected.games.size())+tr(" instalação(ões) encontrados. Confira a seleção."));
 // Diagnostics are explicitly requested by the user, never opened by discovery.
}
string render(const Plan&p,const Cleanup*c=nullptr){
 string s=tr("ORGANIZER ATMOR 1.5.2 | PREVIA\r\n\r\n");
 if(atmor::language==1)s+="Technical reasons and engine messages may remain in Portuguese. Original names and paths are preserved.\r\n\r\n";
 if(atmor::language==2)s+="Los motivos técnicos y mensajes del motor pueden seguir en portugués. Se conservan nombres y rutas originales.\r\n\r\n";
 s+=std::to_string(p.p.mods.size())+tr(" mods | ")+std::to_string(p.separators.size())+tr(" separadores por categoria | ")+std::to_string(p.files)+tr(" arquivos lidos\r\n");
 if(p.localizationLast)s+=tr("MODO: 9 grupos compactos; ordem preservada dentro dos grupos. Outputs por ultimo.\r\n");
 s+=std::to_string(p.moved)+tr(" mods com posicao alterada na proposta. Estados ativo/desativado preservados.\r\n");
 if(c)s+=std::to_string(c->retired.size())+tr(" separadores antigos serao retirados e guardados no backup.\r\n")+std::to_string(c->edits.size())+tr(" perfil(is) com referencias de separadores a atualizar, mantendo a sequencia dos mods.\r\n");
 if(p.unknown){s+=tr("\r\n--- CLASSIFICACOES INCONCLUSIVAS ---\r\n");for(auto&m:p.p.mods)if(m.cat==17)s+="- "+m.name+"\r\n";s+=tr("Para atribuir categorias manualmente, use Caminhos e opções > Editar categorias.\r\n");}
 for(auto&w:p.warnings)s+=tr("AVISO: ")+w+"\r\n";
 s+=tr("\r\n--- REGRAS E MOTIVOS DE PRIORIDADE ---\r\n");if(p.priorityNotes.empty())s+=tr("Nenhuma regra aplicavel ou revisao de prioridades desativada.\r\n");for(auto&note:p.priorityNotes)s+=note+"\r\n";s+=tr("\r\n--- GRUPOS COMPACTOS ---\r\n");
 for(auto&l:p.output){if(ends(l,"_separator"))s+="\r\n[ "+l.substr(1,l.size()-11)+" ]\r\n";else s+=(l[0]=='+'?tr("[ATIVO] "):l[0]=='-'?tr("[DESATIVADO] "):tr("[OFICIAL] "))+l.substr(1)+"\r\n";}
 s+=tr("\r\n--- CLASSIFICACAO: REGRA OU EVIDENCIA ---\r\n");for(size_t i=0;i<p.p.mods.size();i++)s+=p.p.mods[i].name+" => "+tr(cats[p.p.mods[i].cat])+" | "+p.reasons[i]+"\r\n";
 s+=tr("\r\n--- CONFLITOS DE ARQUIVOS SOLTOS (MODS ATIVOS) ---\r\n");
 s+=std::to_string(p.conflicts.size())+tr(" caminhos em conflito. Vencedor NA PROPOSTA, nao garantia de compatibilidade.\r\n");
 size_t shown=0;for(auto&c:p.conflicts){if(shown++>=500){s+=tr("Relatorio visual limitado aos primeiros 500 caminhos. A analise considerou todos.\r\n");break;}s+=tr("\r\nArquivo: ")+c.file+tr("\r\nVencia: ")+c.previousWinner+tr("\r\nVence na proposta: ")+c.winner+tr("\r\nSobrescreve: ");for(auto&n:c.losers)s+=n+"; ";s+="\r\n";}
 if(!p.online.empty()){s+=tr("\r\n--- FONTES ONLINE (INFORMATIVAS; NAO DEFINEM PRIORIDADE) ---\r\n");for(auto&entry:p.online)s+=entry+"\r\n\r\n";}return s;
}
string resourceText(int id){
 auto resource=FindResourceW(nullptr,MAKEINTRESOURCEW(id),RT_RCDATA);
 if(!resource)throw std::runtime_error("Recurso interno do Organizer indisponivel.");
 auto memory=LoadResource(nullptr,resource);
 auto* bytes=static_cast<const char*>(LockResource(memory));
 if(!bytes)throw std::runtime_error("Nao foi possivel ler o recurso interno.");
 return string(bytes,SizeofResource(nullptr,resource));
}
string ruleData(){
 auto custom=appDir/L"regras.tsv";
 auto builtIn=resourceText(201);
 if(!fs::exists(nativePath(custom)))return builtIn;
 auto extra=readFile(nativePath(custom));
 if(extra.compare(0,3,"\xef\xbb\xbf")==0)extra.erase(0,3);
 return builtIn+"\n"+extra;
}
void editCategories(){
 auto custom=appDir/L"regras.tsv";
 if(!fs::exists(nativePath(custom)))writeFile(nativePath(custom),resourceText(202));
 auto path=L"\""+custom.wstring()+L"\"";
 if((INT_PTR)ShellExecuteW(win,L"open",L"notepad.exe",path.c_str(),nullptr,SW_SHOW)<=32)
  throw std::runtime_error("Nao foi possivel abrir regras.tsv. "+custom.u8string());
}
void showHelp(){
 const string help=tr("1. Confira o perfil e a pasta mods e gere a previa.\n"
                  "2. Feche o Mod Organizer 2 e clique em Aplicar com backup.\n"
                  "3. Para desfazer, use Restaurar backup.\n\n"
                  "Se algum mod ficar sem categoria, a previa mantém a ordem original e permite aplicar os separadores.\n"
                  "A consulta Nexus GraphQL e opcional e mostra nome, resumo e versao publicada.");
 if(MessageBoxW(win,wide(help+tr("\n\nQuer salvar as licenças das dependências?")).c_str(),tw("Ajuda"),MB_YESNO|MB_ICONINFORMATION)!=IDYES)return;
 wchar_t path[32768]=L"LICENCAS-Organizer-Atmor.txt";
 OPENFILENAMEW dialog{};
 dialog.lStructSize=sizeof(dialog);
 dialog.hwndOwner=win;
 dialog.lpstrFile=path;
 dialog.nMaxFile=32768;
 dialog.lpstrFilter=L"Text (*.txt)\0*.txt\0";
 dialog.lpstrDefExt=L"txt";
 dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
 if(GetSaveFileNameW(&dialog))writeFile(nativePath(fs::path(path)),resourceText(203));
}
string cleanupSignature(const Cleanup&c){string r;for(auto&s:c.retired)r+=encodeHex(s.name)+":"+(s.meta?encodeHex(*s.meta):"EMPTY")+";";for(auto&e:c.edits)r+=encodeHex(e.path.u8string())+":"+encodeHex(e.before)+":"+encodeHex(e.after)+";";return r;}
struct Analysis{Plan plan;Cleanup cleanup;string data,rules,error;bool doPriority=false;fs::path profile,mods;};
void analyze(){auto p=fs::path(value(profileBox)),m=fs::path(value(modsBox));if(p.empty())throw std::runtime_error(tr("Escolha o modlist.txt do perfil."));if(low(p.filename().u8string())!="modlist.txt")throw std::runtime_error(tr("Selecione o arquivo modlist.txt do perfil."));
 p=fs::absolute(p).lexically_normal();if(!m.empty())m=fs::absolute(m).lexically_normal();cancelPendingDiscovery();updatingDiscovery=true;setPathIfChanged(profileBox,p);if(!m.empty())setPathIfChanged(modsBox,m);updatingDiscovery=false;
 bool online=IsDlgButtonChecked(win,113)==BST_CHECKED;bool priorities=IsDlgButtonChecked(win,114)==BST_CHECKED;ready=false;setBusy(true);text(statusBox,tr("Lendo arquivos, classificando mods e conferindo separadores..."));
 std::thread([p,m,online,priorities](){auto*a=new Analysis;a->profile=p;a->mods=m;try{a->data=readFile(nativePath(p));a->rules=ruleData();a->doPriority=priorities;a->plan=organize(a->data,m,parseRules(a->rules),!m.empty(),PriorityRules{},false,priorities);if(!m.empty())a->cleanup=planCleanup(m,p,a->plan);
 if(online&&!m.empty()){PostMessageW(win,WM_APP+2,0,0);wchar_t local[32768];DWORD len=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);fs::path cache=(len>0&&len<32768?fs::path(local):fs::temp_directory_path())/L"SAMUKXLL-Organizer"/L"cache";consultSources(a->plan,m,cache);}else a->plan.online.push_back(tr("Consulta online desativada. Classificacao e prioridades lidas localmente."));
 }catch(const std::exception&e){a->error=e.what();}PostMessageW(win,WM_APP+1,0,(LPARAM)a);}).detach();}
fs::path backupDir(const fs::path&profile){auto root=profile.parent_path()/L"Atmor-backups";fs::create_directories(root);SYSTEMTIME t;GetLocalTime(&t);wchar_t name[96];swprintf(name,96,L"%04d%02d%02d-%02d%02d%02d-%03d-%lu",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,GetCurrentProcessId());auto dir=root/name;if(!fs::create_directory(dir))throw std::runtime_error(tr("Nao foi possivel criar backup exclusivo."));return dir;}
void atomicWrite(const fs::path&p,const string&s){auto tmp=p;tmp+=L".samukxll-tmp";if(fs::exists(tmp))throw std::runtime_error(tr("Arquivo temporario existente. Revise: ")+tmp.u8string());writeFile(tmp,s);if(!MoveFileExW(tmp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){fs::remove(tmp);throw std::runtime_error(tr("Falha ao substituir modlist. O backup foi preservado."));}}
void apply(){
 if(!ready||selectedMods.empty())throw std::runtime_error(tr("Analise o perfil e a pasta mods antes de aplicar."));
 requireClosed();
 if(value(profileBox)!=selectedProfile.wstring()||value(modsBox)!=selectedMods.wstring()||readFile(nativePath(selectedProfile))!=snapshot||ruleData()!=ruleSnapshot)throw std::runtime_error(tr("Os caminhos, perfil ou regras mudaram. Analise novamente."));
 string question=tr("Aplicar ")+std::to_string(current.separators.size())+tr(" blocos por categoria?\n\n")+std::to_string(current.moved)+tr(" mods mudam de posicao conforme a previa. Confira os motivos de prioridade.\n")+std::to_string(currentCleanup.retired.size())+tr(" separadores antigos serao guardados no backup e retirados da pasta mods.\n")+std::to_string(currentCleanup.edits.size())+tr(" perfil(is) nesta pasta terao suas referencias de separadores atualizadas. Os estados sao preservados. Os outros perfis mantem sua sequencia de mods.\n\nMantenha o MO2 fechado. Separadores sao compartilhados pela instancia.");
 if(prioritiesRequested&&current.unknown)question=tr("Há mods sem categoria. Esta prévia muda apenas os separadores e mantém a ordem dos mods.\n\n")+question;
 if(MessageBoxW(win,wide(question).c_str(),tw("2. Aplicar com backup"),MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
 setBusy(true);text(statusBox,tr("Revalidando a lista e preparando backup dos separadores..."));try{
 auto fresh=organize(snapshot,selectedMods,parseRules(ruleSnapshot),true,PriorityRules{},false,prioritiesRequested);auto cleanup=planCleanup(selectedMods,selectedProfile,fresh);
 if(fresh.serialized!=current.serialized||cleanupSignature(cleanup)!=cleanupSignature(currentCleanup))throw std::runtime_error(tr("Arquivos, perfis ou separadores mudaram. Analise novamente."));
 requireClosed();auto dir=backupDir(selectedProfile);fresh.online=current.online;writeFile(nativePath(dir/L"relatorio.txt"),render(fresh,&cleanup));
 vector<SeparatorData>ensure;for(auto&n:fresh.separators)ensure.push_back({n,string("[General]\r\nmodid=0\r\nversion=1.5.2\r\n")});
 executeTransaction(selectedMods,selectedProfile,dir,cleanup.edits,cleanup.retired,ensure);
 ready=false;text(statusBox,tr("Concluido. Reabra o MO2: categorias separadas, sem separadores antigos vazios."));alert(tr("Organizacao aplicada conforme a previa de prioridades.\nBackup das listas e separadores: ")+dir.u8string());
 }catch(...){setBusy(false);throw;}setBusy(false);
}
void restoreLegacy(const fs::path&file){requireClosed();if(file.filename()!=L"original-modlist.txt")throw std::runtime_error(tr("Escolha original-modlist.txt de um backup criado por este programa."));auto dir=file.parent_path();auto profile=fs::u8path(readFile(dir/L"perfil.txt"));auto mods=fs::u8path(readFile(dir/L"mods.txt"));auto original=readFile(file);auto manifest=lines(readFile(dir/L"criados.txt"));if(!profile.is_absolute()||!mods.is_absolute()||low(profile.filename().u8string())!="modlist.txt")throw std::runtime_error(tr("Caminhos invalidos no backup."));parse(original);if(MessageBoxW(win,(tw("Restaurar este perfil?\n")+profile.wstring()+tw("\n\nA lista atual tambem sera salva. Separadores criados so serao removidos se vazios e nao usados por outros perfis nesta pasta de perfis.")).c_str(),tw("Restaurar backup"),MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
requireClosed();auto safety=backupDir(profile);writeFile(safety/L"antes-da-restauracao.txt",readFile(profile));atomicWrite(profile,original);int kept=0;for(auto&n:manifest){if(!safeName(n)||n.find("SAMUKXLL - ")!=0||!ends(n,"_separator"))continue;auto d=mods/fs::u8path(n);if(!fs::exists(d))continue;bool used=false;try{for(auto&e:fs::directory_iterator(profile.parent_path().parent_path())){auto other=e.path()/L"modlist.txt";if(fs::is_regular_file(other)&&readFile(other).find(n)!=string::npos){used=true;break;}}if(linked(d))used=true;for(auto&e:fs::directory_iterator(d))if(e.path().filename()!=L"meta.ini"||!e.is_regular_file())used=true;if(!used){fs::remove(d/L"meta.ini");fs::remove(d);}else kept++;}catch(...){kept++;}}
ready=false;EnableWindow(applyButton,FALSE);text(statusBox,tr("Backup restaurado. Reabra o MO2."));alert(tr("Lista restaurada. Separadores preservados por uso ou verificacao inconclusiva: ")+std::to_string(kept)+tr(".\nCopia anterior a restauracao: ")+safety.u8string());}
void restore(){requireClosed();auto file=chooseFile(true);if(file.empty())return;
 if(file.filename()!=L"original-modlist.txt")throw std::runtime_error(tr("Selecione original-modlist.txt de um backup."));auto dir=file.parent_path();
 if(!fs::exists(nativePath(dir/L"formato.txt"))){restoreLegacy(file);return;}
 if(trim(readFile(nativePath(dir/L"formato.txt")))!="4")throw std::runtime_error(tr("Formato de backup nao suportado."));
 if(MessageBoxW(win,tw("Restaurar as listas e os separadores deste backup?\n\nOs perfis registrados no backup serao restaurados, inclusive seus estados ativo/desativado. O estado atual sera guardado em outro backup antes da restauracao."),tw("Restaurar listas e separadores"),MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
 auto profile=fs::u8path(readFile(nativePath(dir/L"perfil.txt")));auto safety=backupDir(profile);restoreV4(dir,safety);ready=false;EnableWindow(applyButton,FALSE);text(statusBox,tr("Listas e separadores restaurados. Reabra o MO2."));alert(tr("Restauracao concluida. Copia anterior a restauracao: ")+safety.u8string());
}
// Visual styles are confined to this UI; organization and backup logic are unchanged.
int dpi=96;HFONT titleFont,smallFont,sectionFont,monoFont;
const COLORREF background=RGB(242,244,243),ink=RGB(27,43,45),muted=RGB(86,107,109),navy=RGB(19,39,43),blue=RGB(20,110,106);
HBRUSH bgBrush,whiteBrush,navyBrush;
int px(int n){return MulDiv(n,dpi,96);}
HFONT makeFont(int size,int weight,const wchar_t*name=L"Segoe UI"){return CreateFontW(-px(size),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,name);}
HWND control(const wchar_t*cls,const wchar_t*title,DWORD style,int x,int y,int w,int h,int id){
 bool edit=wcscmp(cls,L"EDIT")==0,button=wcscmp(cls,L"BUTTON")==0;bool checkbox=button&&((style&BS_TYPEMASK)==BS_AUTOCHECKBOX);
 HWND c=CreateWindowExW(edit?WS_EX_CLIENTEDGE:0,cls,title,WS_CHILD|WS_VISIBLE|((edit||button||wcscmp(cls,L"COMBOBOX")==0)?WS_TABSTOP:0)|style|(button&&!checkbox?BS_OWNERDRAW:0),px(x),px(y),px(w),px(h),win,(HMENU)(INT_PTR)id,0,0);
 SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);
 if(edit)SendMessageW(c,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(px(10),px(10)));
 return c;
}
void move(int id,int x,int y,int w,int h){MoveWindow(GetDlgItem(win,id),px(x),px(y),px(w),px(h),TRUE);}
void layout(){RECT r;GetClientRect(win,&r);int w=MulDiv(r.right,96,dpi),h=MulDiv(r.bottom,96,dpi);
 move(201,30,16,w-270,36);move(202,32,57,w-290,22);move(203,w-208,56,176,22);move(123,w-208,18,176,180);
 move(204,36,111,260,24);move(121,300,108,w-336,200);move(208,36,151,w-72,24);
 move(209,36,184,w-72,40);
 for(int id:{101,102,103,104,205,113,114,115,108,116,118,119,120})ShowWindow(GetDlgItem(win,id),advanced?SW_SHOW:SW_HIDE);
 int action=242;
 if(advanced){move(101,36,234,w-244,30);move(103,w-194,234,158,30);move(205,36,274,w-72,22);move(102,36,300,w-244,30);move(104,w-194,300,158,30);move(120,36,342,w-244,190);move(119,w-194,342,158,30);move(114,36,381,w-72,24);move(113,36,410,w-72,24);move(116,36,443,155,32);move(108,201,443,150,32);move(118,361,443,165,32);move(115,536,443,150,32);action=493;}
 move(105,24,action,180,42);move(106,216,action,180,42);move(107,408,action,155,42);move(122,w-208,action,184,42);
 move(207,26,action+65,w-500,25);move(117,w-466,action+60,225,34);move(112,w-229,action+60,130,34);move(109,w-87,action+60,63,34);
 move(110,24,action+106,w-48,std::max(0,h-action-166));move(111,26,h-39,w-52,25);
 text(GetDlgItem(win,122),advanced?tr("Fechar opções"):tr("Caminhos e opções"));InvalidateRect(win,nullptr,TRUE);
}

void rounded(HDC dc,RECT r,COLORREF fill,COLORREF border,int radius=10){
 HBRUSH b=CreateSolidBrush(fill);HPEN p=CreatePen(PS_SOLID,1,border);auto ob=SelectObject(dc,b),op=SelectObject(dc,p);
 RoundRect(dc,r.left,r.top,r.right,r.bottom,px(radius),px(radius));SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(b);DeleteObject(p);
}
void paintWindow(HWND h){PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);FillRect(dc,&r,bgBrush);
 RECT header={0,0,r.right,px(90)};FillRect(dc,&header,navyBrush);
 RECT card={px(24),px(105),r.right-px(24),px(advanced?481:226)};rounded(dc,card,RGB(255,255,255),RGB(220,228,239),14);
 EndPaint(h,&ps);
}
void paintButton(DRAWITEMSTRUCT*d){bool enabled=IsWindowEnabled(d->hwndItem),primary=d->CtlID==105||d->CtlID==106,pressed=d->itemState&ODS_SELECTED;
 COLORREF fill=enabled?(primary?(pressed?RGB(15,85,82):blue):(pressed?RGB(222,236,232):RGB(255,255,255))):RGB(224,231,240);
 COLORREF fg=enabled?(primary?RGB(255,255,255):ink):RGB(117,132,153);
 rounded(d->hDC,d->rcItem,fill,primary?fill:RGB(205,217,232),10);SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,fg);auto old=SelectObject(d->hDC,font);
 auto label=value(d->hwndItem);RECT r=d->rcItem;DrawTextW(d->hDC,label.c_str(),-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
 if(d->itemState&ODS_FOCUS){InflateRect(&r,-px(4),-px(4));DrawFocusRect(d->hDC,&r);}SelectObject(d->hDC,old);
}
#include "diagnostic-ui.hpp"
LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM wp,LPARAM lp){try{switch(msg){case WM_CREATE:{
 win=h;HDC dc=GetDC(h);dpi=GetDeviceCaps(dc,LOGPIXELSY);ReleaseDC(h,dc);
 font=makeFont(15,FW_NORMAL);titleFont=makeFont(26,FW_SEMIBOLD);smallFont=makeFont(13,FW_NORMAL);sectionFont=makeFont(16,FW_SEMIBOLD);monoFont=makeFont(14,FW_NORMAL,L"Consolas");
 bgBrush=CreateSolidBrush(background);whiteBrush=CreateSolidBrush(RGB(255,255,255));navyBrush=CreateSolidBrush(navy);
 control(L"STATIC",L"Organizer Atmor",0,0,0,0,0,201);SendMessageW(GetDlgItem(h,201),WM_SETFONT,(WPARAM)titleFont,TRUE);
 control(L"STATIC",tw("Seus mods em ordem. Com prévia, backup e controle."),0,0,0,0,0,202);
 control(L"STATIC",L"v1.5.2  /  ATMOR",SS_RIGHT,0,0,0,0,203);
 control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,0,0,0,0,123);for(auto label:{L"Português (Brasil)",L"English",L"Español"})SendMessageW(GetDlgItem(h,123),CB_ADDSTRING,0,(LPARAM)label);SendMessageW(GetDlgItem(h,123),CB_SETCURSEL,atmor::language,0);
 control(L"STATIC",tw("01   Perfil do Mod Organizer 2"),0,0,0,0,0,204);
 profileBox=control(L"EDIT",L"",ES_AUTOHSCROLL,0,0,0,0,101);control(L"BUTTON",tw("Buscar modlist.txt"),0,0,0,0,0,103);
 control(L"STATIC",tw("02   Pasta dos mods — a pasta mods da mesma instância do MO2"),0,0,0,0,0,205);
 modsBox=control(L"EDIT",L"",ES_AUTOHSCROLL,0,0,0,0,102);control(L"BUTTON",tw("Buscar pasta..."),0,0,0,0,0,104);
 control(L"BUTTON",tw("Consultar metadados na API Nexus (opcional)"),BS_AUTOCHECKBOX,0,0,0,0,113);CheckDlgButton(win,113,BST_UNCHECKED);control(L"BUTTON",tw("Agrupar em 9 categorias (move mods entre grupos)"),BS_AUTOCHECKBOX,0,0,0,0,114);CheckDlgButton(win,114,BST_CHECKED);control(L"BUTTON",tw("Como funciona"),0,0,0,0,0,115);
 control(L"BUTTON",tw("1. Gerar prévia"),0,0,0,0,0,105);applyButton=control(L"BUTTON",tw("2. Aplicar com backup"),0,0,0,0,0,106);
 control(L"BUTTON",tw("Restaurar backup"),0,0,0,0,0,107);control(L"BUTTON",tw("Editar categorias"),0,0,0,0,0,108);control(L"BUTTON",tw("Ajuda"),0,0,0,0,0,109);EnableWindow(applyButton,FALSE);
 control(L"BUTTON",tw("Detectar instalação"),0,0,0,0,0,116);control(L"BUTTON",tw("Diagnóstico completo"),0,0,0,0,0,117);control(L"BUTTON",tw("Importar crash / log"),0,0,0,0,0,118);control(L"BUTTON",tw("Localizar exe..."),0,0,0,0,0,119);control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,0,0,0,0,120);control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,0,0,0,0,121);control(L"STATIC",tw("Detectando Skyrim automaticamente..."),SS_PATHELLIPSIS,0,0,0,0,208);
 control(L"BUTTON",tw("Caminhos e opções"),0,0,0,0,0,122);control(L"STATIC",tw("9 GRUPOS COMPACTOS\r\nDublagens → Legendas → Prioridades finais · Nemesis / Pandora Output por último"),0,0,0,0,0,209);
 control(L"STATIC",tw("Prévia da organização"),0,0,0,0,0,207);control(L"BUTTON",tw("Copiar"),0,0,0,0,0,112);
 report=control(L"EDIT",tw("PRONTO PARA ORGANIZAR\r\n\r\n1. Confirme o perfil acima e clique em Gerar prévia.\r\n2. Confira o resultado e feche o MO2.\r\n3. Aplique com backup e reabra o MO2.\r\n\r\nA organização usa nove grupos; dublagens, legendas e prioridades finais ficam no fim.\r\nA sequência dentro de cada grupo é preservada. Nada é ativado sozinho.\r\n\r\nPrecisa corrigir caminhos? Abra Caminhos e opções.\r\nQuer investigar dependências ou DLLs? Use Diagnóstico completo.\r\n\r\nSe uma versão anterior alterou sua ordem, restaure primeiro o backup desejado."),ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|ES_AUTOHSCROLL|WS_VSCROLL|WS_HSCROLL,0,0,0,0,110);
 SendMessageW(report,EM_SETLIMITTEXT,16000000,0);SendMessageW(report,WM_SETFONT,(WPARAM)font,TRUE);
 statusBox=control(L"STATIC",tw("Pronto  •  Análise local  •  Backup antes de aplicar"),0,0,0,0,0,111);
 for(int id:{204,205,207})SendMessageW(GetDlgItem(h,id),WM_SETFONT,(WPARAM)sectionFont,TRUE);
 for(int id:{202,203,206,111})SendMessageW(GetDlgItem(h,id),WM_SETFONT,(WPARAM)smallFont,TRUE);
 SendMessageW(profileBox,EM_SETCUEBANNER,FALSE,(LPARAM)tw("Selecione o arquivo modlist.txt do perfil ativo"));
 SendMessageW(modsBox,EM_SETCUEBANNER,FALSE,(LPARAM)L"Ex.: D:\\ModOrganizer\\mods");
 layout();PostMessageW(h,WM_COMMAND,116,0);return 0;}
case WM_TIMER:if(wp==1&&discoveryPending&&!busy){beginDiscovery();}return 0;
case WM_PAINT:paintWindow(h);return 0;
case WM_ERASEBKGND:return 1;
case WM_DRAWITEM:paintButton((DRAWITEMSTRUCT*)lp);return TRUE;
case WM_CTLCOLORBTN:case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:{HDC dc=(HDC)wp;int id=GetDlgCtrlID((HWND)lp);SetBkMode(dc,OPAQUE);
 if(id>=201&&id<=203){SetTextColor(dc,id==201?RGB(255,255,255):RGB(185,212,204));SetBkColor(dc,navy);return (LRESULT)navyBrush;}
 bool white=(id==101||id==102||id==110||id==204||id==205||id==206||id==208||id==209||id==113||id==114);SetTextColor(dc,id==206||id==111?muted:ink);SetBkColor(dc,white?RGB(255,255,255):background);return (LRESULT)(white?whiteBrush:bgBrush);}
case WM_SIZE:if(report)layout();return 0;
case WM_GETMINMAXINFO:((MINMAXINFO*)lp)->ptMinTrackSize={px(840),px(700)};return 0;
case WM_COMMAND:{int id=LOWORD(wp);if((id==101||id==102)&&HIWORD(wp)==EN_CHANGE){ready=false;EnableWindow(applyButton,FALSE);if(id==101&&!updatingDiscovery){gameExe.clear();gameVersion.clear();importedLog.clear();discoveryPending=true;SetTimer(win,1,900,nullptr);}return 0;}if(busy)return 0;switch(id){case 123:if(HIWORD(wp)==CBN_SELCHANGE)changeLanguage((int)SendMessageW(GetDlgItem(win,123),CB_GETCURSEL,0,0));break;case 103:{auto p=chooseFile();if(!p.empty())SetWindowTextW(profileBox,p.c_str());break;}case 104:{auto p=chooseFolder();if(!p.empty())SetWindowTextW(modsBox,p.c_str());break;}case 122:advanced=!advanced;layout();break;case 116:beginDiscovery();break;case 119:{auto p=selectDiagnosticFile();if(!p.empty()){setGame(p);importedLog.clear();}break;}case 120:if(HIWORD(wp)==CBN_SELCHANGE){int i=(int)SendMessageW(GetDlgItem(win,120),CB_GETCURSEL,0,0);if(i>=0&&i<(int)detected.games.size()){setGame(detected.games[i].exe);importedLog.clear();}}break;case 121:if(HIWORD(wp)==CBN_SELCHANGE)chooseDetectedProfile((int)SendMessageW(GetDlgItem(win,121),CB_GETCURSEL,0,0));break;case 117:runDiagnostics();break;case 118:{auto p=selectDiagnosticFile(true);if(!p.empty()){importedLog=p;text(statusBox,tr("Log selecionado. Clique em Dependências e atualizações para conferir."));}break;}case 115:alert(tr("Agrupa em ate 9 separadores, sem grupos vazios ou repetidos: Requisitos e base; Texturas; Graficos; Gameplay, magias, armas e armaduras; Quests; Interface; Dublagens; Legendas; Prioridades finais. A ordem dentro de cada grupo e preservada, com Nemesis/Pandora Output por ultimo. Juntar categorias pode mudar conflitos entre grupos: confira a previa. Desmarcar o agrupamento preserva a ordem exata, mas pode repetir separadores. Nao substitui LOOT nem garante compatibilidade."));break;case 114:case 113:ready=false;EnableWindow(applyButton,FALSE);break;case 112:copyReport();break;case 105:analyze();break;case 106:apply();break;case 107:restore();break;case 108:editCategories();break;case 109:showHelp();break;}return 0;}
case WM_APP+4:{std::unique_ptr<Discovery>d((Discovery*)lp);setBusy(false);applyDiscovery(std::move(*d));return 0;}
case WM_APP+3:{std::unique_ptr<DiagnosticResult> r((DiagnosticResult*)lp);setBusy(false);if(!r->error.empty())throw std::runtime_error(r->error);showDiagnostics(std::move(r->data));text(statusBox,tr("Diagnóstico pronto dentro do programa. Selecione um item para ver a solução e o link."));return 0;}
case WM_APP+2:text(statusBox,tr("Consultando a API Nexus GraphQL. Pode levar cerca de 30 segundos..."));return 0;
case WM_APP+1:{auto*a=(Analysis*)lp;if(a->error.empty()){current=std::move(a->plan);currentCleanup=std::move(a->cleanup);snapshot=a->data;ruleSnapshot=a->rules;prioritiesRequested=a->doPriority;selectedProfile=a->profile;selectedMods=a->mods;ready=!selectedMods.empty();text(report,render(current,&currentCleanup));text(statusBox,ready?(prioritiesRequested&&current.unknown?tr("Prévia pronta com a ordem original. Feche o MO2 para aplicar os separadores."):tr("Previa pronta. Feche o MO2 e clique em 2. Aplicar com backup.")):tr("Selecione a pasta mods e gere outra prévia para aplicar."));}else{ready=false;text(report,tr("ANÁLISE INTERROMPIDA — ORGANIZER ATMOR 1.5.2\r\nNenhum arquivo do perfil foi alterado.\r\n\r\n")+a->error+tr("\r\n\r\nPRÓXIMO PASSO\r\nConfira o caminho e o motivo acima. Use Copiar relatório se precisar de ajuda.\r\nNão apague mods para contornar o erro; primeiro identifique o arquivo indicado."));setBusy(false);text(statusBox,tr("Análise interrompida. O motivo está na área de diagnóstico."));alert(tr("A análise foi interrompida. O mod, o caminho e o motivo estão na área de diagnóstico. Use Copiar relatório para enviar os detalhes."));}delete a;setBusy(false);return 0;}
case WM_CLOSE:if(busy){alert(tr("Aguarde a operacao terminar antes de fechar."));return 0;}DestroyWindow(h);return 0;case WM_DESTROY:for(HFONT f:{font,titleFont,smallFont,sectionFont,monoFont})DeleteObject(f);for(HBRUSH b:{bgBrush,whiteBrush,navyBrush})DeleteObject(b);PostQuitMessage(0);return 0;}}catch(const std::exception&e){alert(e.what());text(statusBox,tr("Operacao interrompida. Consulte a mensagem."));}return DefWindowProcW(h,msg,wp,lp);}
void changeLanguage(int language){
 if(language<0||language>2||busy)return;
 atmor::language=language;saveLanguage();
 ready=false;EnableWindow(applyButton,FALSE);
 if(diagnosticWindow)DestroyWindow(diagnosticWindow);
 SetWindowTextW(GetDlgItem(win,201),tw("Organizer Atmor"));
 SetWindowTextW(GetDlgItem(win,202),tw("Seus mods em ordem. Com prévia, backup e controle."));
 SetWindowTextW(GetDlgItem(win,204),tw("01   Perfil do Mod Organizer 2"));
 SetWindowTextW(GetDlgItem(win,103),tw("Buscar modlist.txt"));
 SetWindowTextW(GetDlgItem(win,205),tw("02   Pasta dos mods — a pasta mods da mesma instância do MO2"));
 SetWindowTextW(GetDlgItem(win,104),tw("Buscar pasta..."));
 SetWindowTextW(GetDlgItem(win,113),tw("Consultar metadados na API Nexus (opcional)"));
 SetWindowTextW(GetDlgItem(win,114),tw("Agrupar em 9 categorias (move mods entre grupos)"));
 SetWindowTextW(GetDlgItem(win,115),tw("Como funciona"));
 SetWindowTextW(GetDlgItem(win,105),tw("1. Gerar prévia"));
 SetWindowTextW(GetDlgItem(win,106),tw("2. Aplicar com backup"));
 SetWindowTextW(GetDlgItem(win,107),tw("Restaurar backup"));
 SetWindowTextW(GetDlgItem(win,108),tw("Editar categorias"));
 SetWindowTextW(GetDlgItem(win,109),tw("Ajuda"));
 SetWindowTextW(GetDlgItem(win,116),tw("Detectar instalação"));
 SetWindowTextW(GetDlgItem(win,117),tw("Diagnóstico completo"));
 SetWindowTextW(GetDlgItem(win,118),tw("Importar crash / log"));
 SetWindowTextW(GetDlgItem(win,119),tw("Localizar exe..."));
 SetWindowTextW(GetDlgItem(win,122),tw("Caminhos e opções"));
 SetWindowTextW(GetDlgItem(win,209),tw("9 GRUPOS COMPACTOS\r\nDublagens → Legendas → Prioridades finais · Nemesis / Pandora Output por último"));
 SetWindowTextW(GetDlgItem(win,207),tw("Prévia da organização"));
 SetWindowTextW(GetDlgItem(win,112),tw("Copiar"));
 SetWindowTextW(report,tw("PRONTO PARA ORGANIZAR\r\n\r\n1. Confirme o perfil acima e clique em Gerar prévia.\r\n2. Confira o resultado e feche o MO2.\r\n3. Aplique com backup e reabra o MO2.\r\n\r\nA organização usa nove grupos; dublagens, legendas e prioridades finais ficam no fim.\r\nA sequência dentro de cada grupo é preservada. Nada é ativado sozinho.\r\n\r\nPrecisa corrigir caminhos? Abra Caminhos e opções.\r\nQuer investigar dependências ou DLLs? Use Diagnóstico completo.\r\n\r\nSe uma versão anterior alterou sua ordem, restaure primeiro o backup desejado."));
 text(statusBox,tr("Idioma alterado. Gere uma nova prévia antes de aplicar."));layout();
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int){HANDLE mutex=CreateMutexW(0,TRUE,L"Local\\SAMUKXLL-MO2-Organizer-v01");if(!mutex||GetLastError()==ERROR_ALREADY_EXISTS){MessageBoxW(0,tw("O organizador ja esta aberto."),L"Organizer Atmor",MB_OK);return 1;}CoInitializeEx(0,COINIT_APARTMENTTHREADED);INITCOMMONCONTROLSEX common{sizeof(common),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};InitCommonControlsEx(&common);SetProcessDPIAware();wchar_t path[32768];GetModuleFileNameW(0,path,32768);appDir=fs::path(path).parent_path();loadLanguage();WNDCLASSW c{};c.lpfnWndProc=proc;c.hInstance=instance;c.hCursor=LoadCursorW(0,IDC_ARROW);c.hbrBackground=nullptr;c.hIcon=(HICON)LoadImageW(instance,MAKEINTRESOURCEW(101),IMAGE_ICON,0,0,LR_DEFAULTSIZE);if(!c.hIcon)c.hIcon=(HICON)LoadImageW(nullptr,(appDir/L"logo.ico").c_str(),IMAGE_ICON,0,0,LR_LOADFROMFILE|LR_DEFAULTSIZE);c.lpszClassName=L"SamukxllOrganizer";RegisterClassW(&c);HWND h=CreateWindowW(c.lpszClassName,L"Organizer Atmor 1.5.2",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1100,880,0,0,instance,0);RECT area;SystemParametersInfoW(SPI_GETWORKAREA,0,&area,0);int ww=std::min(px(1100),(int)(area.right-area.left)-32),hh=std::min(px(880),(int)(area.bottom-area.top)-32);SetWindowPos(h,nullptr,area.left+((area.right-area.left)-ww)/2,area.top+((area.bottom-area.top)-hh)/2,ww,hh,SWP_NOZORDER);ShowWindow(h,SW_SHOW);MSG m;while(GetMessageW(&m,0,0,0)>0){if(!(diagnosticWindow&&IsDialogMessageW(diagnosticWindow,&m))&&!IsDialogMessageW(h,&m)){TranslateMessage(&m);DispatchMessageW(&m);}}CoUninitialize();CloseHandle(mutex);return 0;}
