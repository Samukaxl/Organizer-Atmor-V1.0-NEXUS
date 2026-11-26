#include "online.hpp"
#include <cassert>
#include <iostream>
int main(int argc, char** argv) {
 auto request = nlohmann::json::parse(graphqlRequestBody({"skyrimspecialedition", "12604"}));
 assert(request["variables"]["gameId"] == "1704");
 assert(request["variables"]["modId"] == "12604");
 assert(request["operationName"] == "AtmorMod");
 bool rejected = false;
 try { graphqlRequestBody({"skyrimspecialedition", "1\"} mutation"}); }
 catch (const std::invalid_argument&) { rejected = true; }
 assert(rejected);
 assert(nexusGameId("skyrim") == "110" && nexusGameId("fallout4") == "1151");
 assert(nexusGameId("newvegas") == "130" && nexusGameId("oblivion") == "101");
 auto info = parseGraphqlResponse(R"({"data":{"mod":{"name":"Magia \u00e9 \ud83d\udd25","summary":"Uma \"magia\"\nNova","version":"2.0"}}})");
 assert(info.title == u8"Magia é 🔥" && info.description == "Uma \"magia\" Nova" && info.version == "2.0");
 for (const string invalid : {"<html><title>Mod</title></html>", "{}", "null", "[]", "{",
  R"({"data":{"mod":null}})", R"({"data":{"mod":{"name":12,"summary":"","version":""}}})",
  R"({"errors":[{"message":"Forbidden"}],"data":{"mod":{"name":"Partial","summary":"","version":"1"}}})"})
  assert(parseGraphqlResponse(invalid).title.empty());
 PageInfo emptyFields{"Mod", "", "API Nexus GraphQL consultada", ""};
 auto restored = parseGraphqlResponse(graphqlCacheBody(emptyFields));
 assert(restored.title == "Mod" && restored.description.empty() && restored.version.empty());
 assert(parseGraphqlResponse(graphqlCacheBody(info)).title == info.title);
 assert(metadataText(u8"aéz", 2) == "a");
 if (argc > 1) {
  auto live = parseGraphqlResponse(readFile(argv[1]));
  assert(live.title == "SkyUI" && !live.version.empty());
 }
 std::cout << "GraphQL request, Unicode, invalid/partial data and cache tests passed\n";
}
