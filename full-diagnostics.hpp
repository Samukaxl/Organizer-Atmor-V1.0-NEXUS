#pragma once
#include "diagnostics.hpp"
struct DiagnosticFile{fs::path path;string owner;};
inline string diagnosticText(const Diagnostics&d){
 string out="SAMUKXLL 1.4.0 - DIAGNOSTICO COMPLETO\r\nPerfil: "+d.profile+"\r\nSkyrim: "+d.version+"\r\nExecutavel: "+d.exe+"\r\nCabecalhos conferidos: "+std::to_string(d.checked)+"\r\n\r\n";
 out+="Leitura local. Nao executa DLLs, nao testa Novo Jogo e nao garante ausencia de crashes. Logs podem pertencer a outro perfil ou a uma tentativa antiga. Revise caminhos pessoais antes de compartilhar.\r\n";
 for(auto&r:d.rows)out+="\r\n["+r.status+"] "+r.item+"\r\n"+r.detail+"\r\n"+(r.url.empty()?"":r.url+"\r\n");return out;
}
inline void addCrashEvidence(Diagnostics&d,const string&log,const string&source){
 if(log.empty())return;
 d.rows.push_back({"LOG / NAO CONFIRMA ESTADO ATUAL","Crash mais recente",source+"\r\nA data e o perfil devem corresponder a ultima tentativa. Nomes na pilha nao identificam sozinhos o culpado.","",""});
 bool fatal=false;size_t count=0;
 for(auto&line:lines(log)){
  auto n=low(line);
  if(n.find("missing masters")!=string::npos){fatal=true;d.rows.push_back({"ERRO REGISTRADO NO LOG","Carregamento interrompido por mestres",line+"\r\nCompare com os erros AUSENTE da leitura atual. Um log antigo nao prova que o arquivo ainda falta.","",""});}
  if((fatal&&hasAny(n,{"not found","missing:","disable:","intermediate:"}))||hasAny(n,{"crash time:","rash time:","skyrim sse v","process uptime:","unhandled exception","open menus:"})){
   d.rows.push_back({"EVIDENCIA DO LOG","Trecho do crash",line,"",""});if(++count>=60)break;
  }
 }
 if(!fatal)d.rows.push_back({"INCONCLUSIVO","Causa do crash","O log nao apresenta a secao conhecida de mestres fatais. Access violation, nomes de DLLs e uso de RAM nao bastam para determinar a causa. Teste num perfil copiado e compare um novo log.","",""});
}
inline void completeDiagnostics(Diagnostics&d,const fs::path&profile,const fs::path&mods,const fs::path&data,const fs::path&overwrite,const string&crash="",const string&crashSource=""){
 std::map<string,vector<DiagnosticFile>>files;auto parsed=parse(readFile(nativePath(profile)));
 auto layer=[&](const fs::path&root,const string&owner){try{for(auto&relative:scanFiles(root)){
  // Focus on executable plugins, translations and generated behaviours, not every texture.
  if(isPlugin(relative)||ends(relative,".dll")||ends(relative,".strings")||ends(relative,".dlstrings")||ends(relative,".ilstrings")||ends(relative,".hkx"))files[low(relative)].push_back({root/fs::u8path(relative),owner});
 }}catch(const std::exception&e){d.rows.push_back({"LEITURA INCOMPLETA",owner,e.what(),"",""});}};
 std::error_code ec;if(!data.empty()&&fs::is_directory(nativePath(data),ec))layer(data,"Data do jogo");
 for(auto&m:parsed.mods)if(m.state=='+')layer(mods/fs::u8path(m.name),m.name);
 if(!overwrite.empty()&&fs::is_directory(nativePath(overwrite),ec))layer(overwrite,"overwrite");
 d.rows.push_back({"ESCOPO","Pastas conferidas","Mods: "+mods.u8string()+"\r\nData: "+data.u8string()+"\r\nOverwrite: "+overwrite.u8string()+"\r\nPrioridade: Data -> mods ativos -> overwrite. Arquivos ocultos e links nao sao seguidos; conteudo BSA/BA2 nao e inspecionado. Saidas personalizadas de executaveis fora destes caminhos nao sao cobertas.","",""});
 size_t conflicts=0;for(auto&[file,providers]:files)if(providers.size()>1){conflicts++;if(ends(file,".hkx")||ends(file,".strings")||ends(file,".dlstrings")||ends(file,".ilstrings"))continue;
  auto&win=providers.back();string detail="Vencedor: "+win.owner+"\r\nArquivo efetivo: "+win.path.u8string()+"\r\nOrdem dos provedores (ultimo vence):\r\n";
  for(auto&p:providers)detail+="- "+p.owner+"\r\n";
  detail+="Sobrescrita confirmada; nao comprova incompatibilidade. Se uma traducao substituir ESP/ESM, confira se corresponde a versao do original.";
  if(isPlugin(file)){try{auto before=readMasters(providers.front().path),after=readMasters(win.path);if(before!=after){detail+="\r\nMAST DIFERENTES entre primeiro provedor e vencedor. Mestres do vencedor: ";for(auto&m:after)detail+=m+"; ";}}catch(const std::exception&e){detail+="\r\nComparacao incompleta: "+string(e.what());}}
  d.rows.push_back({"SOBRESCRITA / REVISAR",file,detail,"",""});
 }
 d.rows.push_back({"INVENTARIO","Arquivos relevantes",std::to_string(files.size())+" caminhos de plugins/DLLs/textos/animacoes; "+std::to_string(conflicts)+" caminhos com mais de um provedor. Duplicidade nao significa erro.","",""});
 auto readProfile=[&](const char*name){auto p=profile.parent_path()/name;try{return readFile(nativePath(p));}catch(...){return string("Arquivo nao disponivel");}};
 for(auto name:{"modlist.txt","plugins.txt","loadorder.txt"})d.rows.push_back({"ARQUIVO DO PERFIL",name,readProfile(name),"",""});
 addCrashEvidence(d,crash,crashSource);
 if(crash.empty())d.rows.push_back({"INCONCLUSIVO","Crash log","Nenhum crash foi encontrado nos caminhos consultados. Importe um arquivo .log ou .txt em Caminhos e opcoes.","",""});
 size_t errors=0,incomplete=0;for(auto&r:d.rows){if(r.status=="AUSENTE"||r.status=="DEPENDENCIA DLL AUSENTE"||r.status=="INCOMPATIBILIDADE DECLARADA"||r.status=="ATUALIZAR SKSE"||r.status=="MOD DESATIVADO"||r.status=="PLUGIN DESATIVADO")errors++;if(r.status=="LEITURA INCOMPLETA"||r.status=="NAO VERIFICADO")incomplete++;}
 d.rows.insert(d.rows.begin(),{"RESUMO / PROXIMO PASSO","Comece aqui",std::to_string(errors)+" pendencias de arquivo, ativacao ou declaracao encontradas; "+std::to_string(incomplete)+" verificacoes incompletas.\r\n1. Resolva AUSENTE e mestres exigidos; nao renomeie ESP para ESM.\r\n2. Confira DLLs com incompatibilidade declarada e a versao do SKSE.\r\n3. Revise sobrescritas, principalmente traducoes de plugins.\r\n4. Execute LOOT no mesmo perfil e teste Novo Jogo pelo SKSE.\r\n5. Se fechar, gere este diagnostico novamente e envie o TXT.\r\nZero erros detectados nao comprova que o jogo inicia.","",""});
}
