#define main retained_dialog_main
#include "dialog_session.cpp"
#undef main
#include <limits>
static void setupModels(JavaGameSession& game){game.pendingPackFonts.clear();game.packFontError.clear();game.resourcePackConnection=2;game.resourcePackGeneration=3;game.resourcePacks={JavaGameSession::ResourcePack{}};game.resourcePacks[0].id="practice";strings={{"pack_fonts.id","practice"},{"pack_fonts.textures.0.rgba_base64","/////w=="},{"pack_fonts.models.0.id","oraxen:test"},{"pack_fonts.models.0.tints.0.kind","custom_model_data"},{"pack_fonts.model_errors.0","Unsupported animation"}};arrays={{"pack_fonts.textures",1},{"pack_fonts.fonts",0},{"pack_fonts.models",1},{"pack_fonts.models.0.quads",1},{"pack_fonts.models.0.tints",1},{"pack_fonts.models.0.quads.0.vertices",4},{"pack_fonts.model_errors",1}};numbers={{"pack_fonts.connection",2},{"pack_fonts.generation",3},{"pack_fonts.textures.0.width",1},{"pack_fonts.textures.0.height",1},{"pack_fonts.models.0.quads.0.texture",0},{"pack_fonts.models.0.quads.0.tint",0},{"pack_fonts.models.0.tints.0.color",16777215},{"pack_fonts.models.0.tints.0.index",1},{"pack_fonts.models.0.tints.0.default_color",65280}};for(const char* field:{"rotation","translation","scale"}){std::string p=std::string("pack_fonts.models.0.gui.")+field;arrays[p]=3;for(int i=0;i<3;++i)numbers[p+"."+std::to_string(i)]=std::string(field)=="scale"?1:0;}for(int v=0;v<4;++v){std::string p="pack_fonts.models.0.quads.0.vertices."+std::to_string(v);arrays[p]=5;for(int k=0;k<5;++k)numbers[p+"."+std::to_string(k)]=k<3?16:1;}eventKind=TCPE_JAVA_PACK_FONTS;}
static void applyModels(JavaGameSession& g){pending=true;g.tick();}
int main(){JavaGameSession game;setupModels(game);applyModels(game);assert(game.pendingPackFonts.size()==1);const auto& bundle=game.pendingPackFonts[0];assert(bundle.models.size()==1&&bundle.models[0].id=="oraxen:test"&&bundle.models[0].quads[0].vertices[0][0]==16&&bundle.modelErrors.size()==1);
 assert(bundle.models[0].tints.size()==1&&bundle.models[0].tints[0].index==1&&bundle.models[0].tints[0].defaultColor==0x00ff00);
 auto display=[&](){setupModels(game);arrays["pack_fonts.models.0.display"]=1;strings["pack_fonts.models.0.display.0.context"]="firstperson_righthand";for(const char* field:{"rotation","translation","scale"}){std::string key=std::string("pack_fonts.models.0.display.0.transform.")+field;arrays[key]=3;for(int axis=0;axis<3;++axis)numbers[key+"."+std::to_string(axis)]=std::string(field)=="scale"?.5:axis+1;}};
 display();applyModels(game);assert(game.pendingPackFonts.size()==1);assert(game.pendingPackFonts[0].models[0].display.at("firstperson_righthand").translation[2]==3);assert(game.pendingPackFonts[0].models[0].display.at("firstperson_righthand").scale[0]==.5f);
 for(double invalid:{5.,std::numeric_limits<double>::infinity()}){display();numbers["pack_fonts.models.0.display.0.transform.scale.0"]=invalid;applyModels(game);assert(game.pendingPackFonts.empty());}
 display();strings["pack_fonts.models.0.display.0.context"]="unknown";applyModels(game);assert(game.pendingPackFonts.empty());
 display();arrays["pack_fonts.models.0.display"]=10;applyModels(game);assert(game.pendingPackFonts.empty());
 for(double invalid:{-1.,16777216.,1.5,std::numeric_limits<double>::infinity()}){setupModels(game);numbers["pack_fonts.models.0.tints.0.color"]=invalid;applyModels(game);assert(game.pendingPackFonts.empty());}
 for(double invalid:{-1.,1024.,1.5}){setupModels(game);numbers["pack_fonts.models.0.tints.0.index"]=invalid;applyModels(game);assert(game.pendingPackFonts.empty());}
 setupModels(game);strings["pack_fonts.models.0.tints.0.kind"]="unknown";applyModels(game);assert(game.pendingPackFonts.empty());
 setupModels(game);numbers["pack_fonts.models.0.quads.0.texture"]=1;applyModels(game);assert(game.pendingPackFonts.empty());
 setupModels(game);arrays["pack_fonts.models.0.quads.0.vertices"]=3;applyModels(game);assert(game.pendingPackFonts.empty());
 setupModels(game);numbers["pack_fonts.models.0.quads.0.vertices.0.0"]=std::numeric_limits<double>::quiet_NaN();applyModels(game);assert(game.pendingPackFonts.empty());
 setupModels(game);numbers["pack_fonts.models.0.gui.scale.0"]=std::numeric_limits<double>::infinity();applyModels(game);assert(game.pendingPackFonts.empty());
 setupModels(game);arrays["pack_fonts.models.0.quads"]=65537;applyModels(game);assert(game.pendingPackFonts.empty());
 setupModels(game);numbers["pack_fonts.generation"]=2;applyModels(game);assert(game.pendingPackFonts.empty());
 std::cout<<"Pack model handoff: Java16 vertices, GUI transforms, texture bounds, array limits, finite fields, generation gating and explicit errors PASS\n";
}
