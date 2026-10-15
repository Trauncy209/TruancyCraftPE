#ifndef TCPE_JAVA_INVENTORY_VIEW_PREFERENCE_H
#define TCPE_JAVA_INVENTORY_VIEW_PREFERENCE_H
#include "../../OptionsFile.h"
// One explicit client preference, independent of server titles, contents or IDs.
// OptionsFile preserves this key when other settings or saved servers are saved.
namespace JavaInventoryViewPreference {
inline bool read(){bool value=false;const auto pairs=OptionsFile().getOptionStrings();for(size_t i=0;i+1<pairs.size();i+=2)if(pairs[i]=="mp_java_storage_view")value=pairs[i+1]=="true"||pairs[i+1]=="1";return value;}
inline bool& storage(){static bool value=read();return value;}
inline void setStorage(bool value){storage()=value;OptionsFile().save(StringVector{"mp_java_storage_view:"+std::string(value?"true":"false")});}
}
#endif
