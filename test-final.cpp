#include "core.hpp"
#include "diagnostics.hpp"
#include <cassert>
#include <iostream>
#include <random>
void checkBlocks(const Plan&p){
 int previous=-1,count=0;size_t index=0;
 for(const auto&entry:p.output){
  if(entry[0]=='*')continue;
  if(ends(entry,"_separator")){previous=-1;++count;continue;}
  assert(index<p.p.mods.size());int category=separatorCategory(p.p.mods[index++]);
  if(previous!=-1)assert(previous==category);previous=category;
 }
 assert(index==p.p.mods.size()&&count==(int)p.separators.size());
}
int main(){
 assert(inferCategory("Unofficial Skyrim Special Edition Patch",{}).cat==18);
 assert(inferCategory("Address Library",{"skse/plugins/a.dll"}).cat==0);
 assert(inferCategory("SkyUI",{"skse/plugins/a.dll"}).cat==12);
 assert(inferCategory("Quest Legendas",{}).cat==14);
 assert(parseRules("18\tBase\n").at("Base")==18);
 auto distinct=organize("+Pandora Output\n+Dublagem PT-BR\n+Legendas\n+Anim\n+Interface\n+Requisito\n+Base\n",{},{{"Base",18},{"Requisito",0},{"Interface",12},{"Anim",9},{"Legendas",14}},false,{},false,true);
 checkBlocks(distinct);assert(distinct.separators.size()==6);
 auto repeated=organize("+A2\n+UI\n+A1\n",{},{{"A1",9},{"UI",12},{"A2",9}},false);
 checkBlocks(repeated);assert(repeated.separators.size()==3);assert(repeated.p.mods[0].name=="A1"&&repeated.p.mods[2].name=="A2");

 string input="+Characters\n+Quest PT-BR\n+Late Patch\n-Dublagem PT-BR\n+Original\n+Other\n";
 Rules baseRules={{"Other",5},{"Original",5},{"Late Patch",5},{"Characters",5}};auto p=organize(input,{},baseRules,false,{},false,true);vector<string>expected={"Other","Original","Late Patch","Characters","Dublagem PT-BR","Quest PT-BR"};for(size_t i=0;i<expected.size();i++)assert(p.p.mods[i].name==expected[i]);assert(p.p.mods[4].state=='-');assert(p.separators.back().find("Legendas")!=string::npos);assert(p.serialized.find("+Quest PT-BR")<p.serialized.find("+Characters"));assert(organize(p.serialized,{},baseRules,false,{},false,true).serialized==p.serialized);
 assert(inferCategory("Original Quest",{"strings/original_english.strings"}).cat!=14);
 assert(localizedName("Quest Traduzido Portugues BR"));
 std::mt19937 gen(10);for(int k=0;k<100;k++){string input;Rules rules;vector<string>base,translations;for(int i=0;i<60;i++){string name="Mod "+std::to_string(i)+(gen()%3==0?" PT-BR":"");rules[name]=localizedName(name)?14:5;string entry=(gen()%5?"+":"-")+name;input=entry+"\n"+input;(localizedName(name)?translations:base).push_back(entry);}auto p=organize(input,{},rules,false,{},false,true);checkBlocks(p);vector<string>expected=base;expected.insert(expected.end(),translations.begin(),translations.end());assert(p.p.mods.size()==expected.size());for(size_t i=0;i<expected.size();i++)assert(string(1,p.p.mods[i].state)+p.p.mods[i].name==expected[i]);assert(organize(p.serialized,{},rules,false,{},false,true).serialized==p.serialized);}
 // Every category, shuffled repeatedly: one block per group, stable order, no lost states.
 for(int trial=0;trial<100;trial++){
  string data;Rules rules;vector<string>expected[11];
  for(int i=0;i<120;i++){
   string name="Test "+std::to_string(i);int cat=gen()%19;if(cat==17)cat=5;
   if(i==30)name="Pandora Output";
   char state=gen()%4?'+':'-';rules[name]=cat;Mod m{name,state,cat};
   string entry=string(1,state)+name;data=entry+"\n"+data;expected[compactPriority(m)].push_back(entry);
  }
  auto plan=organize(data,{},rules,false,{},false,true);checkBlocks(plan);
  assert(plan.separators.size()<=9);size_t j=0;
  for(auto& group:expected)for(auto& entry:group){assert(string(1,plan.p.mods[j].state)+plan.p.mods[j].name==entry);++j;}
  assert(j==120);assert(organize(plan.serialized,{},rules,false,{},false,true).serialized==plan.serialized);
 }
 assert(searchMod("Unknown","1.7.104").empty());assert(masterPage("unknown.esm","1.7.104").empty());assert(knownDllPage("wheeler.dll","1.7.104.0").find("167380?tab=files")!=string::npos);assert(masterPage("BSAssets.esm","1.7.104").find("198585")!=string::npos);
 Rules outRules={{"Original",5},{"Late Base",5}};auto out=organize("+Late Base\n+Quest PT-BR\n+Pandora Output\n+Nemesis Output\n+Nemesis Unlimited Behavior Engine\n+Original\n",{},outRules,false,{},false,true);
 vector<string>want={"Nemesis Unlimited Behavior Engine","Original","Late Base","Quest PT-BR","Nemesis Output","Pandora Output"};for(size_t j=0;j<want.size();j++)assert(out.p.mods[j].name==want[j]);assert(out.separators.back().find("Prioridades finais")!=string::npos);assert(organize(out.serialized,{},outRules,false,{},false,true).serialized==out.serialized);assert(animationOutput("PANDORA_OUTPUT"));assert(animationOutput("Saída Nemesis"));assert(!animationOutput("Pandora Behaviour Engine Plus"));assert(!animationOutput("Nemesis Unlimited Behavior Engine"));
 auto root=fs::temp_directory_path()/"samukxll-output-test";fs::remove_all(root);for(auto n:{"Base","Translation PTBR","Pandora Output"}){fs::create_directories(root/n/"meshes");writeFile(root/n/"meshes"/"a.hkx","x");}auto win=organize("+Translation PTBR\n+Base\n+Pandora Output\n",root,{},true,{},false,true);assert(win.conflicts[0].winner=="Pandora Output");assert(win.conflicts[0].previousWinner=="Translation PTBR");fs::remove_all(root);
 std::cout<<"PASS compact groups <=9, 200 randomized lists, stable group order/states, idempotence, outputs and before/after conflicts\n";
}
