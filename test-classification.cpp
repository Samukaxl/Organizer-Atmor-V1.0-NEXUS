#include "core.hpp"
#include "transaction.hpp"
#include <cassert>
#include <iostream>
int main(){
 vector<string>mixed={"interface/skyui/config.swf","skse/plugins/mod.dll","textures/a.dds","sound/voice/a.esp/npc/a.fuz","meshes/a.hkx","scripts/a.pex","package.esp"};
 for(auto name:{"Missives","Missives - Worldspace Additions","Beyond Skyrim - Bruma","Wyrmstooth","College of Winterhold - Quest Expansion","There Is No Umbra - Chapter III"})assert(inferCategory(name,mixed).cat==4);
 for(auto name:{"Valhalla Combat","ValhallaCombat V1.3.5 AE 1.7.104.0","Precision","SCAR - Skyrim Combos AI Revolution"})assert(inferCategory(name,mixed).cat==8);
 for(auto name:{"Wheeler - Quick Action Wheel Of Skyrim","Edge UI - Wheeler Reskin","Infinity UI","RaceMenu AE NG 1.7.104","Main Menu Redone"})assert(inferCategory(name,mixed).cat==12);
 assert(inferCategory("Audio Overhaul for Skyrim (4.1.2)",mixed).cat==11);
 assert(inferCategory("Skyrim Special Edition - BR atualizada",mixed).cat==14);
 assert(inferCategory("Expressive Facegen Morphs SE",mixed).cat==13);
 assert(inferCategory("Missives PTBR",{}).cat==14);
 assert(inferCategory("Dublagem Wyrmstooth",{}).cat==15);
 assert(inferCategory("Renamed",{"skse/plugins/valhallacombat.dll"}).cat==8);
 assert(inferCategory("Renamed",{"wyrmstooth.esp"}).cat==4);
 assert(inferCategory("Mystery",mixed).cat==17);
 assert(inferCategory("Mystery",{"skse/plugins/mystery.dll"}).cat==17);
 assert(inferCategory("Mystery",{"interface/a.swf","mystery.esp"}).cat==17);
 assert(inferCategory("Mystery",{"textures/a.dds","mystery.bsa","mystery.esp"}).cat==17);
 assert(inferCategory("Unrelated Reskin",{}).cat==17);
 assert(inferCategory("Luxury Mystery",{}).cat==17);
 auto root=fs::temp_directory_path()/"samukxll-classification-regression";fs::remove_all(root);
 fs::create_directories(root/"mods"/"Mystery");fs::create_directories(root/"mods"/"Wheeler");
 writeFile(root/"mods"/"Mystery"/"mystery.dll","fixture");writeFile(root/"mods"/"Wheeler"/"wheeler.dll","fixture");
 auto oldSeparator=root/"mods"/"Antigo_separator";
 fs::create_directory(oldSeparator);
 string original="+Mystery\n-Wheeler\n-Antigo_separator\n";
 auto plan=organize(original,root/"mods",{},true,{},false,true);
 assert(plan.unknown==1&&!plan.localizationLast&&!plan.prioritiesEnabled&&plan.moved==0);
 assert(modEntries(plan.serialized)==modEntries(original));
 auto fixed=organize(original,root/"mods",{{"Mystery",0}},true,{},false,true);
 assert(fixed.unknown==0&&fixed.localizationLast&&fixed.p.mods.front().name=="Mystery");
 assert(fixed.p.mods.back().state=='-');
 auto profile=root/"profiles"/"Default"/"modlist.txt";fs::create_directories(profile.parent_path());writeFile(profile,original);
 // Revalidation and the normal transaction must accept an inconclusive preview.
 auto fresh=organize(original,root/"mods",{},true,{},false,true);
 assert(fresh.serialized==plan.serialized);
 auto cleanup=planCleanup(root/"mods",profile,fresh);
 vector<SeparatorData> ensure;
 for(const auto& name:fresh.separators)ensure.push_back({name,string("[General]\n")});
 executeTransaction(root/"mods",profile,root/"backup",cleanup.edits,cleanup.retired,ensure);
 assert(readFile(profile)==fresh.serialized);
 assert(modEntries(readFile(profile))==modEntries(original));
 assert(!fs::exists(oldSeparator));
 assert(readFile(root/"backup"/"original-modlist.txt")==original);
 restoreV4(root/"backup",root/"safety");
 assert(readFile(profile)==original&&fs::is_directory(oldSeparator));

 // Fully classified lists can still be reordered by category.
 cleanup=planCleanup(root/"mods",profile,fixed);
 ensure.clear();
 for(const auto& name:fixed.separators)ensure.push_back({name,string("[General]\n")});
 executeTransaction(root/"mods",profile,root/"grouped-backup",cleanup.edits,cleanup.retired,ensure);
 assert(readFile(profile)==fixed.serialized);
 restoreV4(root/"grouped-backup",root/"grouped-safety");
 assert(readFile(profile)==original);
 fs::remove_all(root);std::cout<<"PASS semantic classification, misleading assets, renamed signatures, inconclusive and grouped apply/backup/restore\n";
}
