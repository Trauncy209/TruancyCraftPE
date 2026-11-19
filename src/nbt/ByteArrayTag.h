#ifndef COM_MOJANG_NBT__ByteArrayTag_H__
#define COM_MOJANG_NBT__ByteArrayTag_H__

//package com.mojang.nbt;

/* import java.io.* */

#include "Tag.h"
#include <string>

typedef struct TagMemoryChunk {
	TagMemoryChunk()
	:	data(NULL),
		len(0)
	{}
	void* data;
	int len;
} TagMemoryChunk;

class ByteArrayTag: public Tag
{
	typedef Tag super;
public:
    TagMemoryChunk data;
private:
    bool ownsData = false;
public:
    ~ByteArrayTag() { deleteChildren(); }
    void deleteChildren() {
        if (ownsData) delete[] static_cast<char*>(data.data);
        data.data = NULL; data.len = 0; ownsData = false;
    }

    ByteArrayTag(const std::string& name)
    :   super(name)
	{
    }

    ByteArrayTag(const std::string& name, TagMemoryChunk data)
    :   super(name),
		data(data)
	{
    }

    char getId() const {
        return TAG_Byte_Array;
    }

    std::string toString() const {
        std::stringstream ss;
        ss << "[" << data.len << " bytes]";
        return ss.str();
    }

    //@Override
    bool equals(const Tag& rhs) const {
        if (super::equals(rhs)) {
			ByteArrayTag& o = (ByteArrayTag&) rhs;
			if (data.len != o.data.len)
				return false;

			return memcmp(data.data, o.data.data, data.len) == 0;
        }
        return false;
    }

    //@Override
    Tag* copy() const {
        ByteArrayTag* result = new ByteArrayTag(getName());
        if (data.len > 0 && data.data) {
            result->data.len = data.len;
            result->data.data = new char[data.len];
            result->ownsData = true;
            memcpy(result->data.data, data.data, data.len);
        }
        return result;
    }

    void write(IDataOutput* dos) /*throws IOException*/ {
        dos->writeInt(data.len);
        dos->writeBytes(data.data, data.len);
    }

    void load(IDataInput* dis) /*throws IOException*/ {
        deleteChildren();
        int length = dis->readInt();
        if (length < 0 || length > 64 * 1024 * 1024) { errorState |= TAGERR_OUT_OF_BOUNDS; return; }
        data.len = length;
        if (!length) return;
        data.data = new char[length];
        ownsData = true;
        dis->readBytes(data.data, length);
    }
};

#endif /*COM_MOJANG_NBT__ByteArrayTag_H__*/
