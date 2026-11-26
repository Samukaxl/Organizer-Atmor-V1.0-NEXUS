#include "core.hpp"
#include <cassert>
#include <iostream>
int main(){
 for(auto& [key, values]:atmor::catalog()){assert(!key.empty());assert(!values[0].empty());assert(!values[1].empty());}
 assert(inferCategory("Apocalypse - Traducción",{}).cat==14);
 assert(inferCategory("Wyrmstooth - doblaje español",{}).cat==15);
 assert(inferCategory("Apocalypse English Translation",{}).cat==14);
 assert(!localizedName("English Longbow"));
 assert(!localizedName("Spanish Moss"));
 assert(localizedName("Quest subtítulos"));
 assert(canonicalModName("Apocalypse - Traducción español")==canonicalModName("Apocalypse"));
 std::vector<std::string> names;
 for(int language=0;language<3;++language){atmor::language=language;
  auto p=organize("+Pandora Output\n+Quest Translation\n+SkyUI\n",{},{{"Quest Translation",14},{"SkyUI",12}},false,{},false,true);
  std::vector<std::string> order;for(auto&m:p.p.mods)order.push_back(m.name);
  if(language==0)names=order;else assert(order==names);
  assert(p.p.mods.back().name=="Pandora Output");
  auto again=organize(p.serialized,{},{{"Quest Translation",14},{"SkyUI",12}},false,{},false,true);
  assert(again.serialized==p.serialized);
  assert(std::string(tr("Ajuda"))==(language==0?"Ajuda":language==1?"Help":"Ayuda"));
  assert(std::wstring(tw("Buscar pasta...")).size()>0);
 }
 std::cout<<"PASS: 3 locales, nonempty catalog, localized separators, stable order, idempotence, Spanish/English recognition, protected mod names\n";
}
