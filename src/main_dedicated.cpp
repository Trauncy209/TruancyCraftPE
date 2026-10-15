#include "network/NativeOperators.h"
#include <sys/stat.h>
#include "NinecraftApp.h"
#include "AppConstants.h"
#include "AppPlatform.h"
#include "client/User.h"
#include "network/RakNetInstance.h"
#include "network/ServerSideNetworkHandler.h"
#include "world/level/Level.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/LevelSettings.h"
#include "platform/time.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <limits>
#include <csignal>
#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

namespace {
volatile std::sig_atomic_t stopping=0;
void requestStop(int){stopping=1;}
struct Settings {
    std::string directory=".",world="world",name="TruancyCraftPE Server",consolePath;
    int port=25565,players=16,autosave=60,mode=GameType::Survival;
    long seed=getEpochTimeS();
};
long number(const std::string& value,long minimum,long maximum){
    size_t end=0;long result=std::stol(value,&end);
    if(end!=value.size()||result<minimum||result>maximum)throw std::runtime_error("Invalid number: "+value);
    return result;
}
void help(){std::cout<<"TruancyCraftPE native server "<<APP_VERSION_STRING<<"\n"
    <<"Usage: ./tcpe-server [options]\n"
    <<"  --port 25565        Native UDP gameplay and TCP server listing\n"
    <<"  --directory .      Directory containing saved worlds\n"
    <<"  --console PATH     Owner-only local console FIFO (optional)\n"
    <<"  --world world      World folder name (loaded if it already exists)\n"
    <<"  --name NAME        Server-list name\n"
    <<"  --seed NUMBER      Seed for a new world\n"
    <<"  --creative         Create a Creative world (default: Survival)\n"
    <<"  --max-players 16   Maximum connected players (1–64)\n"
    <<"  --autosave 60      Save interval in seconds (10–3600)\n"
    <<"  --help             Show this help\n"
    <<"Console: save, list, stop, help. Ctrl+C/SIGTERM save and stop.\n"
    <<"Optional icon: 64x64 server-icon.png in the working directory.\n";
}
void directories(const std::string& path){
    if(path.empty())throw std::runtime_error("Empty data directory");
    for(size_t i=1;i<=path.size();++i)if(i==path.size()||path[i]=='/'){
        auto part=path.substr(0,i);if(mkdir(part.c_str(),0755)&&errno!=EEXIST)throw std::runtime_error("Cannot create directory: "+part);
        struct stat info{};if(stat(part.c_str(),&info)||!S_ISDIR(info.st_mode))throw std::runtime_error("Not a directory: "+part);
    }
}
struct WorldLock {
    int fd=-1;
    explicit WorldLock(const std::string& path){fd=open(path.c_str(),O_CREAT|O_RDWR|O_CLOEXEC,0600);if(fd<0||flock(fd,LOCK_EX|LOCK_NB)){if(fd>=0)close(fd);fd=-1;throw std::runtime_error("World is already in use, or its lock cannot be opened");}}
    ~WorldLock(){if(fd>=0)close(fd);}
};
class DedicatedApp:public NinecraftApp {
public:
    ~DedicatedApp(){close();}
    void finishLoading(){if(level&&!isGeneratingLevel&&!_hasSignaledGeneratingLevelFinished)_levelGenerated();}
    void save(){if(level){level->saveGame();level->getChunkSource()->saveAll(true);}}
    void close(){if(level){finishLoading();leaveGame();}}
};
}
int main(int argc,char** argv){
    std::cout.setf(std::ios::unitbuf);std::cerr.setf(std::ios::unitbuf);
    try {
        Settings settings;
        for(int i=1;i<argc;++i){std::string key=argv[i];if(key=="--help"){help();return 0;}if(key=="--creative"){settings.mode=GameType::Creative;continue;}if(key=="--survival"){settings.mode=GameType::Survival;continue;}
            if(i+1==argc)throw std::runtime_error("Missing value for "+key);std::string value=argv[++i];
            if(key=="--port")settings.port=number(value,1,65535);
            else if(key=="--directory"||key=="--externalpath")settings.directory=value;
            else if(key=="--world"||key=="--leveldir")settings.world=value;
            else if(key=="--name"||key=="--levelname")settings.name=value;
            else if(key=="--console")settings.consolePath=value;
            else if(key=="--seed")settings.seed=number(value,std::numeric_limits<int>::min(),std::numeric_limits<int>::max());
            else if(key=="--max-players")settings.players=number(value,1,64);
            else if(key=="--autosave")settings.autosave=number(value,10,3600);
            else throw std::runtime_error("Unknown option: "+key);
        }
        if(settings.world.empty()||settings.world=="."||settings.world==".."||settings.world.find_first_of("/\\\r\n")!=std::string::npos)throw std::runtime_error("World must be a folder name");
        if(settings.name.empty()||settings.name.size()>120||settings.name.find_first_of("\r\n")!=std::string::npos)throw std::runtime_error("Server name must contain 1–120 bytes on one line");
        directories(settings.directory);WorldLock lock(settings.directory+"/"+settings.world+".lock");
        std::signal(SIGINT,requestStop);std::signal(SIGTERM,requestStop);std::signal(SIGQUIT,requestStop);std::signal(SIGPIPE,SIG_IGN);
        AppPlatform platform;AppContext context{};context.platform=&platform;context.doRender=false;
        auto app=std::unique_ptr<DedicatedApp>(new DedicatedApp());app->externalStoragePath=settings.directory;app->externalCacheStoragePath=settings.directory;static_cast<App*>(app.get())->init(context);app->user->name=settings.name;app->options.username=settings.name;
        delete app->netCallback;app->netCallback=new ServerSideNetworkHandler(app.get(),app->raknetInstance);
        if(!app->raknetInstance->host(settings.name,settings.port,settings.players))throw std::runtime_error("Cannot bind native UDP port "+std::to_string(settings.port));
        std::cout<<"Loading native TCPE world "<<settings.world<<"...\n";
        app->selectLevel(settings.world,settings.name,LevelSettings(settings.seed,settings.mode));app->finishLoading();app->save();
        if(!static_cast<RakNetInstance*>(app->raknetInstance)->listingActive())throw std::runtime_error("Cannot bind TCP listing port "+std::to_string(settings.port));
        std::cout<<"READY: "<<settings.name<<" | native TCPE "<<APP_VERSION_STRING<<" | port "<<settings.port<<" | max "<<settings.players<<"\n";
        auto next=std::chrono::steady_clock::now();auto nextSave=next+std::chrono::seconds(settings.autosave);bool console=true;std::string line;int consoleFd=STDIN_FILENO;
        if(!settings.consolePath.empty()){if(mkfifo(settings.consolePath.c_str(),0600)&&errno!=EEXIST)throw std::runtime_error("Cannot create console FIFO");consoleFd=open(settings.consolePath.c_str(),O_RDWR|O_NONBLOCK|O_NOFOLLOW);struct stat st{};if(consoleFd<0||fstat(consoleFd,&st)||!S_ISFIFO(st.st_mode)||st.st_uid!=geteuid()||(st.st_mode&0077))throw std::runtime_error("Console must be an owner-only FIFO");}
        while(!stopping){
            app->update();auto now=std::chrono::steady_clock::now();if(now>=nextSave){app->save();nextSave=now+std::chrono::seconds(settings.autosave);std::cout<<"World saved\n";}
            if(console){pollfd input{consoleFd,POLLIN,0};if(poll(&input,1,0)>0){char data[256];ssize_t count=read(consoleFd,data,sizeof(data));if(count<=0)console=false;else for(ssize_t i=0;i<count;++i){if(data[i]=='\n'){if(line.compare(0,4,"/op ")==0||line.compare(0,3,"op ")==0||line.compare(0,6,"/deop ")==0||line.compare(0,5,"deop ")==0){bool grant=line.find("deop")>1;std::string name=line.substr(line.find(' ')+1);std::cout<<(NativeOperators::change(name,grant)?(grant?"Operator granted: ":"Operator revoked: "):"Invalid username or operator file write failed: ")<<name<<"\n";}else if(line=="stop")stopping=1;else if(line=="save"){app->save();std::cout<<"World saved\n";}else if(line=="list"){std::cout<<app->level->players.size()<<" player(s):";for(auto* player:app->level->players)std::cout<<" "<<player->name;std::cout<<"\n";}else if(line=="help")std::cout<<"Commands: save, list, stop, help, /op username, /deop username\n";else if(!line.empty())std::cout<<"Unknown command; use help\n";line.clear();}else if(data[i]!='\r'){if(line.size()<4096)line+=data[i];else{line.clear();std::cout<<"Console line too long\n";}}}}}
            next+=std::chrono::milliseconds(50);if(next<now-std::chrono::seconds(1))next=now;std::this_thread::sleep_until(next);
        }
        std::cout<<"Saving world and stopping...\n";app->close();app.reset();platform.finish();std::cout<<"Server stopped cleanly\n";return 0;
    }catch(const std::exception& error){std::cerr<<"Server error: "<<error.what()<<"\n";return 1;}
}
