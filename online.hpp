#pragma once
#include "core.hpp"
#include <chrono>
#include <cctype>
#ifdef _WIN32
#include <winhttp.h>
#endif
struct Source{string game,id;string url()const{return "https://www.nexusmods.com/"+game+"/mods/"+id;}bool valid()const{return !game.empty()&&!id.empty();}};
inline string trim(string s){auto a=s.find_first_not_of(" \t\r\n\"");if(a==string::npos)return "";auto b=s.find_last_not_of(" \t\r\n\"");return s.substr(a,b-a+1);}
inline Source sourceFromMeta(const string&data){string game,id;bool general=false;for(auto&line:lines(data)){auto l=trim(line);if(l.empty())continue;if(l[0]=='['){general=low(l)=="[general]";continue;}if(!general)continue;auto eq=l.find('=');if(eq==string::npos)continue;auto key=low(trim(l.substr(0,eq))),val=trim(l.substr(eq+1));if(key=="gamename")game=low(val);if(key=="modid")id=val;}
 const std::map<string,string>games={{"skyrim special edition","skyrimspecialedition"},{"skyrimse","skyrimspecialedition"},{"skyrimspecialedition","skyrimspecialedition"},{"skyrim","skyrim"},{"fallout 4","fallout4"},{"fallout4","fallout4"},{"fallout new vegas","newvegas"},{"falloutnv","newvegas"},{"oblivion","oblivion"}};
 auto g=games.find(game);if(g==games.end()||id.empty()||id.size()>10||id.find_first_not_of("0123456789")!=string::npos||id.find_first_not_of('0')==string::npos)return {};return {g->second,id};
}
// Nexus GraphQL V2 schema: https://graphql.nexusmods.com/
// Only public metadata is requested. No HTML page retrieval or fallback.
#include "vendor/nlohmann/json.hpp"
struct PageInfo {
 string title, description, status, version;
};
inline string nexusGameId(const string& domain) {
 const std::map<string,string> ids = {
  {"skyrimspecialedition", "1704"}, {"skyrim", "110"},
  {"fallout4", "1151"}, {"newvegas", "130"}, {"oblivion", "101"}
 };
 auto found = ids.find(domain);
 return found == ids.end() ? "" : found->second;
}
inline string graphqlRequestBody(const Source& source) {
 auto gameId = nexusGameId(source.game);
 if (gameId.empty() || source.id.empty() || source.id.size() > 10 ||
     source.id.find_first_not_of("0123456789") != string::npos ||
     source.id.find_first_not_of('0') == string::npos)
  throw std::invalid_argument("Identificacao Nexus invalida");
 return nlohmann::json{
  {"operationName", "AtmorMod"},
  {"query", "query AtmorMod($gameId: ID!, $modId: ID!) { mod(gameId: $gameId, modId: $modId) { name summary version } }"},
  {"variables", {{"gameId", gameId}, {"modId", source.id}}}
 }.dump();
}
inline string metadataText(string text, size_t limit) {
 // Keep Unicode intact when shortening long API metadata for the report.
 if (text.size() > limit) {
  while (limit > 0 && (static_cast<unsigned char>(text[limit]) & 0xc0) == 0x80) --limit;
  text.resize(limit);
 }
 for (char& c : text) if (static_cast<unsigned char>(c) < 32) c = ' ';
 return text;
}
inline PageInfo parseGraphqlResponse(const string& body) {
 try {
  auto json = nlohmann::json::parse(body);
  if (!json.is_object()) return {"", "", "Resposta GraphQL invalida"};
  if (json.contains("errors") && !json["errors"].is_null()) {
   // Reject partial data too; it must never become a successful cached lookup.
   return {"", "", "Erro GraphQL; metadados indisponiveis"};
  }
  if (!json.contains("data") || !json["data"].is_object() ||
      !json["data"].contains("mod") || !json["data"]["mod"].is_object())
   return {"", "", "Mod indisponivel na API Nexus"};
  const auto& mod = json["data"]["mod"];
  if (!mod.contains("name") || !mod["name"].is_string() ||
      !mod.contains("summary") || !mod["summary"].is_string() ||
      !mod.contains("version") || !mod["version"].is_string())
   return {"", "", "Resposta GraphQL com campos invalidos"};
  auto name = metadataText(mod["name"].get<string>(), 1500);
  if (name.empty()) return {"", "", "Resposta GraphQL sem nome do mod"};
  return {name, metadataText(mod["summary"].get<string>(), 6000),
          "API Nexus GraphQL consultada", metadataText(mod["version"].get<string>(), 300)};
 } catch (const nlohmann::json::exception&) {
  return {"", "", "Resposta JSON invalida da API Nexus"};
 }
}
#ifdef _WIN32
struct HttpHandle {
 HINTERNET h;
 explicit HttpHandle(HINTERNET value) : h(value) {}
 ~HttpHandle() { if (h) WinHttpCloseHandle(h); }
 operator HINTERNET() const { return h; }
};
inline PageInfo fetchSource(const Source& source) {
 string body = graphqlRequestBody(source);
 HttpHandle session(WinHttpOpen(L"Organizer-Atmor/1.5.2 (Nexus GraphQL metadata)",
  WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
 if (!session.h) return {"", "", "Sem conexao HTTP"};
 WinHttpSetTimeouts(session, 2500, 2500, 2500, 2500);
 HttpHandle connection(WinHttpConnect(session, L"api.nexusmods.com", INTERNET_DEFAULT_HTTPS_PORT, 0));
 if (!connection.h) return {"", "", "Falha de conexao"};
 HttpHandle request(WinHttpOpenRequest(connection, L"POST", L"/v2/graphql", nullptr,
  WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
 if (!request.h) return {"", "", "Falha na consulta"};
 DWORD disable = WINHTTP_DISABLE_REDIRECTS;
 if (!WinHttpSetOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &disable, sizeof(disable)))
  return {"", "", "Falha ao configurar consulta segura"};
 if (!WinHttpSendRequest(request, L"Content-Type: application/json\r\nAccept: application/json\r\n", (DWORD)-1L,
     body.data(), static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0) ||
     !WinHttpReceiveResponse(request, nullptr))
  return {"", "", "API indisponivel (rede ou tempo limite)"};
 DWORD status = 0, size = sizeof(status);
 if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
     WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX))
  return {"", "", "Resposta HTTP invalida"};
 if (status != 200) return {"", "", "HTTP " + std::to_string(status) + "; API indisponivel"};
 string response;
 char buffer[16384];
 auto start = std::chrono::steady_clock::now();
 for (;;) {
  DWORD got = 0;
  if (!WinHttpReadData(request, buffer, sizeof(buffer), &got))
   return {"", "", "Falha durante leitura da API"};
  if (!got) break;
  if (response.size() + got > 512 * 1024)
   return {"", "", "Resposta da API acima do limite"};
  if (std::chrono::steady_clock::now() - start > std::chrono::seconds(8))
   return {"", "", "Tempo limite de leitura da API"};
  response.append(buffer, got);
 }
 return parseGraphqlResponse(response);
}
#endif
inline string graphqlCacheBody(const PageInfo& info) {
 return nlohmann::json{{"data", {{"mod", {{"name", info.title},
  {"summary", info.description}, {"version", info.version}}}}}}.dump();
}
inline void consultSources(Plan& plan, const fs::path& root, const fs::path& cache) {
#ifdef _WIN32
 struct Job { Source source; vector<string> mods; };
 vector<Job> jobs;
 std::map<string, size_t> keys;
 int missing = 0;
 for (auto& mod : plan.p.mods) {
  Source source;
  try {
   auto meta = nativePath(root / fs::u8path(mod.name) / L"meta.ini");
   if (fs::is_regular_file(meta) && fs::file_size(meta) < 1024 * 1024)
    source = sourceFromMeta(readFile(meta));
  } catch (...) { }
  if (!source.valid()) { ++missing; continue; }
  auto key = source.game + "-" + source.id;
  auto existing = keys.find(key);
  if (existing == keys.end()) {
   keys[key] = jobs.size();
   jobs.push_back({source, {mod.name}});
  } else jobs[existing->second].mods.push_back(mod.name);
 }
 std::error_code ec;
 fs::create_directories(cache, ec);
 int requests = 0;
 bool blocked = false;
 auto start = std::chrono::steady_clock::now();
 for (const auto& job : jobs) {
  PageInfo info;
  bool cached = false;
  auto file = cache / fs::u8path("graphql-v1-" + job.source.game + "-" + job.source.id + ".json");
  try {
   auto age = fs::file_time_type::clock::now() - fs::last_write_time(file);
   if (age >= std::chrono::hours(0) && age < std::chrono::hours(24) && fs::file_size(file) < 16000) {
    info = parseGraphqlResponse(readFile(file));
    cached = !info.title.empty();
   }
  } catch (...) { }
  if (!cached) {
   if (blocked || requests >= 40 || std::chrono::steady_clock::now() - start > std::chrono::seconds(30)) {
    info.status = "Consulta adiada (limite de tempo/acesso); analise local mantida";
   } else {
    ++requests;
    try {
     info = fetchSource(job.source);
     blocked = hasAny(info.status, {"HTTP 401", "HTTP 403", "HTTP 429", "Erro GraphQL"});
     if (!info.title.empty()) {
      try { writeFile(file, graphqlCacheBody(info)); } catch (...) { }
     }
    } catch (...) { info.status = "Consulta indisponivel; analise local mantida"; }
   }
  }
  string row = job.source.url() + "\r\n" + (cached ? "Cache de ate 24h. " : "") + info.status;
  for (const auto& name : job.mods) row += "\r\nMod: " + name;
  if (!info.title.empty()) row += "\r\nTitulo: " + info.title;
  if (!info.description.empty()) row += "\r\nResumo Nexus: " + info.description;
  if (!info.version.empty()) row += "\r\nVersao publicada no Nexus: " + info.version +
   " (confira variante e runtime antes de atualizar)";
  plan.online.push_back(row);
 }
 plan.online.push_back("Nexus GraphQL: " + std::to_string(jobs.size()) + " mods identificados; " +
  std::to_string(missing) + " sem identificacao suficiente. Consulta informativa.");
#else
 plan.online.push_back("Consulta online disponivel no executavel Windows.");
#endif
}
