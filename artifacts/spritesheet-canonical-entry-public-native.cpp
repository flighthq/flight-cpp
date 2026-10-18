#include <flight/spritesheet_formats/spritesheet_detect.hpp>
#include <flight/spritesheet_formats/cocos_plist_serialize.hpp>
#include <flight/spritesheet/spritesheet_data.hpp>
#include <flight/json_scalar.hpp>
#include <iostream>
#include <stdexcept>
#include <set>
namespace f=flight::spritesheet_formats;
namespace t=flight::types;
using flight::String;
using E=t::SpritesheetFormatEntry;
template<class T> using View=flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<T>>>>;
template<class T> View<T> view(const std::shared_ptr<T>& p){ return View<T>(p); }
int checks=0;
void check(bool b,const char* label){ ++checks; if(!b)throw std::runtime_error(label); }
template<class R> auto entry_of(const R& descriptor){return flight::row_get<flight::RowKey<"entry">>(descriptor);}
View<t::SpritesheetImportOptions> options(flight::Array<View<t::SpritesheetFormatDescriptor>> formats){
 return flight::make_structural_ref<flight::RowReadonly<flight::RowOf<flight::Ref<t::SpritesheetImportOptions>>>>(flight::row_field<flight::RowKey<"formats">>(std::optional{formats}));
}
View<t::SpritesheetFormatDescriptor> descriptor(String kind,flight::Ref<E> entry){
 return flight::make_structural_ref<flight::RowReadonly<flight::RowOf<flight::Ref<t::SpritesheetFormatDescriptor>>>>(flight::row_field<flight::RowKey<"entry">>(entry),flight::row_field<flight::RowKey<"kind">>(kind));
}
int main(){try{
const String TEXTURE_PACKER_JSON("{\"frames\":{},\"meta\":{\"app\":\"https://www.codeandweb.com/texturepacker\",\"format\":\"RGBA8888\",\"image\":\"atlas.png\",\"scale\":1,\"size\":{\"h\":128,\"w\":128},\"version\":\"1.0\"}}");
const String ASEPRITE_JSON("{\"frames\":{},\"meta\":{\"app\":\"https://www.aseprite.org/\",\"format\":\"RGBA8888\",\"frameTags\":[],\"image\":\"sprite.png\",\"scale\":\"1\",\"size\":{\"h\":64,\"w\":64},\"version\":\"1.3\"}}");
const String STARLING_XML("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<TextureAtlas imagePath=\"atlas.png\">\n  <SubTexture name=\"hero\" x=\"0\" y=\"0\" width=\"64\" height=\"64\"/>\n</TextureAtlas>");
const String LIBGDX_ATLAS("\natlas.png\nsize: 128, 64\nformat: RGBA8888\nfilter: Linear, Linear\nrepeat: none\nhero\n  rotate: false\n  xy: 0, 0\n  size: 64, 64\n  orig: 64, 64\n  offset: 0, 0\n  index: -1\n");
const String COCOS_PLIST("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n<plist version=\"1.0\">\n<dict>\n  <key>frames</key>\n  <dict>\n    <key>hero.png</key>\n    <dict>\n      <key>textureRect</key>\n      <string>{{0,0},{64,64}}</string>\n    </dict>\n  </dict>\n  <key>metadata</key>\n  <dict>\n    <key>textureFileName</key>\n    <string>atlas.png</string>\n  </dict>\n</dict>\n</plist>");
const String PARTICLE_DESIGNER_PLIST("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<plist version=\"1.0\">\n<dict>\n  <key>maxParticles</key><integer>200</integer>\n  <key>emitterType</key><integer>0</integer>\n  <key>particleLifespan</key><real>1.0</real>\n  <key>textureFileName</key><string>spark.png</string>\n</dict>\n</plist>");
const String COCOS_PLIST_FORMAT_0("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<plist version=\"1.0\">\n<dict>\n  <key>frames</key>\n  <dict>\n    <key>hero.png</key>\n    <dict>\n      <key>frame</key><string>{{0,0},{64,64}}</string>\n      <key>offset</key><string>{0,0}</string>\n      <key>sourceSize</key><string>{64,64}</string>\n    </dict>\n  </dict>\n</dict>\n</plist>");

const String seam("acme.SeamTest");
check(!f::get_spritesheet_format(seam),"seam initially absent");
auto empty=flight::spritesheet::create_spritesheet_data();
auto seam_entry=flight::make_ref<E>(E{.detect=[](String){return false;},.parse=[empty](String,flight::Ref<t::SpritesheetParseOptions>){return empty;}});
f::apply_spritesheet_import_options(options({descriptor(seam,seam_entry)}));
auto seam_result=f::get_spritesheet_format(seam);
check(seam_result.has_value(),"named format installed");
check(seam_result->identity()==view(seam_entry).identity(),"original public entry owner retained");
f::unregister_spritesheet_format(seam);
check(!f::get_spritesheet_format(seam),"named format removed");
f::unregister_spritesheet_format(t::spritesheet_format_kind_starling);
f::apply_spritesheet_import_options(view(flight::make_ref<t::SpritesheetImportOptions>()));
check(!f::get_spritesheet_format(t::spritesheet_format_kind_starling),"empty options stay empty");
f::apply_spritesheet_import_options(options({f::starling_spritesheet_format}));
check(f::get_spritesheet_format(t::spritesheet_format_kind_starling)->identity()==view(entry_of(f::starling_spritesheet_format)).identity(),"restored built-in same entry");
const std::pair<String,String> detection[]={
 {TEXTURE_PACKER_JSON,t::spritesheet_format_kind_texture_packer},
 {ASEPRITE_JSON,t::spritesheet_format_kind_aseprite},
 {STARLING_XML,t::spritesheet_format_kind_starling},
 {COCOS_PLIST,t::spritesheet_format_kind_cocos_plist},
 {COCOS_PLIST_FORMAT_0,t::spritesheet_format_kind_cocos_plist},
 {LIBGDX_ATLAS,t::spritesheet_format_kind_libgdx_atlas}};
for(const auto& [text,kind]:detection)check(f::detect_spritesheet_format(text)==kind,"original fixture detector");
check(!f::detect_spritesheet_format(PARTICLE_DESIGNER_PLIST),"ParticleDesigner not claimed");
const char* negative_plists[]={
 "<?xml version=\"1.0\"?><plist version=\"1.0\"><dict></dict></plist>",
 "<?xml version=\"1.0\"?><plist version=\"1.0\"><dict><key>metadata</key><dict/></dict></plist>",
 "<?xml version=\"1.0\"?><plist version=\"1.0\"><dict><key>frames</key><array/></dict></plist>",
 "<?xml version=\"1.0\"?><plist version=\"1.0\"><dict><key>note</key><string>frames</string></dict></plist>"};
for(const auto* text:negative_plists)check(!f::detect_spritesheet_format(String(text)),"non-sheet plist refused by detector");
for(const auto& text:flight::Array<String>{COCOS_PLIST,String("<?xml version=\"1.0\"?><plist version=\"1.0\"><dict><key>frames</key><dict/></dict></plist>")}){
 auto sheet=f::parse_cocos_plist_spritesheet(text);
 check(f::detect_spritesheet_format(f::serialize_cocos_plist_spritesheet(view(sheet)))==t::spritesheet_format_kind_cocos_plist,"actual Cocos writer agrees with detector");
}
check(f::detect_spritesheet_format(COCOS_PLIST.replace(String("<key>hero.png</key>"),String("<key>frames</key>")))==t::spritesheet_format_kind_cocos_plist,"frame named frames");
check(!f::detect_spritesheet_format(String("completely unknown content")),"unknown detection");
auto texture_entry=f::get_spritesheet_format(t::spritesheet_format_kind_texture_packer);
check(texture_entry.has_value(),"known entry resolves");
check(!f::get_spritesheet_format(String("acme.NonExistent")),"unknown entry absent");
const String custom("test.CustomFormat");
auto custom_data=flight::spritesheet::create_spritesheet_data(); custom_data->image_file=String("custom.png");
int detect_calls=0,parse_calls=0;
flight::Ref<t::SpritesheetParseOptions> forwarded;
auto custom_entry=flight::make_ref<E>(E{
 .detect=[&](String text){++detect_calls;return text.starts_with(String("CUSTOM:"));},
 .parse=[&](String,flight::Ref<t::SpritesheetParseOptions> opts){++parse_calls;forwarded=opts;return custom_data;}});
f::register_spritesheet_format(custom,custom_entry);
check(f::get_spritesheet_format(custom)->identity()==view(custom_entry).identity(),"register entry identity");
check(f::detect_spritesheet_format(String("CUSTOM: data"))==custom,"custom detector original path");
auto custom_opts=flight::make_ref<t::SpritesheetParseOptions>(); custom_opts->frame_duration=250;
auto parsed_custom=f::parse_spritesheet(String("CUSTOM: data"),std::nullopt,custom_opts);
check(parsed_custom&&parsed_custom.value()==custom_data,"custom parse result original owner");
check(forwarded==custom_opts&&parse_calls==1&&detect_calls>0,"original callback/options forwarding");
auto kinds=f::get_spritesheet_format_kinds();check(kinds.size()==6,"kind enumeration adds exactly one");
for(std::size_t i=1;i<kinds.size();++i)check(kinds[i-1]<kinds[i],"kinds sorted");
f::unregister_spritesheet_format(custom);
check(!f::get_spritesheet_format_kinds().includes(custom),"kind enumeration removes entry");
check(!f::parse_spritesheet(String("unknown format content here")),"unknown parse result");
auto packer=f::parse_spritesheet(TEXTURE_PACKER_JSON);check(packer&&packer.value()->image_file==String("atlas.png"),"actual TexturePacker registry parser");
auto aseprite=f::parse_spritesheet(ASEPRITE_JSON);check(aseprite&&aseprite.value()->image_file==String("sprite.png"),"actual Aseprite registry parser");
auto starling=f::parse_spritesheet(STARLING_XML);check(starling&&starling.value()->frames[0]->name==String("hero"),"actual Starling registry parser");
auto dims=flight::make_ref<t::SpritesheetParseOptions>();dims->image_height=128;dims->image_width=256;
auto sized=f::parse_spritesheet(STARLING_XML,t::spritesheet_format_kind_starling,dims);
check(sized&&sized.value()->image_height==128&&sized.value()->image_width==256,"Starling dimensions forwarded");
check(f::parse_spritesheet(TEXTURE_PACKER_JSON,t::spritesheet_format_kind_texture_packer).has_value(),"explicit format override");
const String tag_doc("{\"frames\":{},\"meta\":{\"app\":\"https://www.codeandweb.com/texturepacker\",\"format\":\"RGBA8888\",\"frameTags\":[{\"direction\":\"forward\",\"from\":0,\"name\":\"run\",\"to\":0}],\"image\":\"atlas.png\",\"scale\":1,\"size\":{\"h\":64,\"w\":64},\"version\":\"1.0\"}}");
check(f::parse_spritesheet(tag_doc,t::spritesheet_format_kind_texture_packer,custom_opts).has_value(),"frameDuration helper forwarding");
check(texture_entry.value()->detect(ASEPRITE_JSON),"Aseprite overlaps TexturePacker detector");
check(f::detect_spritesheet_format(ASEPRITE_JSON)==t::spritesheet_format_kind_aseprite,"Aseprite priority retained");
std::set<std::string> seen;std::size_t aseprite_index=99,packer_index=99;
for(std::size_t i=0;i<f::spritesheet_all_formats.size();++i){
 const auto descriptor=f::spritesheet_all_formats[i];const auto kind=flight::row_get<flight::RowKey<"kind">>(descriptor);const auto entry=entry_of(descriptor);
 check(f::get_spritesheet_format(kind)->identity()==view(entry).identity(),"all original descriptor/registry entry identities");
 seen.insert(kind.to_utf8()); if(kind==t::spritesheet_format_kind_aseprite)aseprite_index=i;if(kind==t::spritesheet_format_kind_texture_packer)packer_index=i;
}
check(aseprite_index<packer_index,"original preset Aseprite ordering");check(seen.size()==f::spritesheet_all_formats.size(),"each kind named once");check(seen.size()==f::get_spritesheet_format_kinds().size(),"preset exactly covers registry");
// Additional alias/lifetime controls supplement the original source cases.
f::apply_spritesheet_import_options(options({descriptor(custom,custom_entry)}));
auto retained=f::get_spritesheet_format(custom).value();
custom_entry->detect=[](String text){return text==String("LIVE");};
check(retained->detect(String("LIVE")),"same native entry live callback replacement");
f::unregister_spritesheet_format(custom);custom_entry.reset();
check(retained->detect(String("LIVE")),"resolved entry keeps original owner alive after unregister");
check(!f::detect_spritesheet_format(String("LIVE")),"unregistered entry removed from detection");
std::cout<<checks<<" whole-original public registry checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
