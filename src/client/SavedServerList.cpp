#include "SavedServerList.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace {
const char* const kSavedServerPrefix = "mp_saved_server_";
const int kDefaultPort = 25565;

std::string stripNewlines(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        char c = value[i];
        if (c != '\r' && c != '\n') {
            out.push_back(c);
        }
    }
    return out;
}

bool parseSavedServerKey(const std::string& key, int& index, std::string& field) {
    const std::string prefix(kSavedServerPrefix);
    if (key.find(prefix) != 0) {
        return false;
    }

    std::string suffix = key.substr(prefix.size());
    size_t separator = suffix.find('_');
    if (separator == std::string::npos) {
        return false;
    }

    std::string indexText = suffix.substr(0, separator);
    if (indexText.empty()) {
        return false;
    }

    for (size_t i = 0; i < indexText.size(); ++i) {
        if (indexText[i] < '0' || indexText[i] > '9') {
            return false;
        }
    }

    index = std::atoi(indexText.c_str());
    field = suffix.substr(separator + 1);
    return !field.empty();
}
}

std::string SavedServerList::getSettingsPath() {
#ifdef __APPLE__
    return "./Documents/options.txt";
#else
    return "options.txt";
#endif
}

std::string SavedServerList::trim(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && (unsigned char)value[start] <= ' ') {
        ++start;
    }

    size_t end = value.size();
    while (end > start && (unsigned char)value[end - 1] <= ' ') {
        --end;
    }

    return value.substr(start, end - start);
}

std::string SavedServerList::sanitizeAddress(const std::string& value) {
    return trim(stripNewlines(value));
}

std::string SavedServerList::sanitizeName(const std::string& value, const std::string& fallbackAddress) {
    std::string trimmed = trim(stripNewlines(value));
    return trimmed.empty() ? fallbackAddress : trimmed;
}

int SavedServerList::sanitizePort(int value) {
    return value > 0 && value <= 65535 ? value : kDefaultPort;
}

std::vector<SavedServerEntry> SavedServerList::load() {
    std::vector<SavedServerEntry> entries;
    FILE* file = std::fopen(getSettingsPath().c_str(), "r");
    if (!file) {
        return entries;
    }

    char lineBuffer[1024];
    std::map<int, SavedServerEntry> indexedEntries;
    while (std::fgets(lineBuffer, sizeof(lineBuffer), file)) {
        std::string line = stripNewlines(lineBuffer);
        if (line.empty()) {
            continue;
        }

        size_t colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        int index = -1;
        std::string field;
        if (!parseSavedServerKey(key, index, field)) {
            continue;
        }

        if (field == "name") {
            indexedEntries[index].name = value;
        } else if (field == "address") {
            indexedEntries[index].address = value;
        } else if(field=="protocol"){
            indexedEntries[index].native=value=="tcpe";
        } else if (field == "port") {
            indexedEntries[index].port = std::atoi(value.c_str());
        }
    }

    std::fclose(file);

    for (std::map<int, SavedServerEntry>::const_iterator it = indexedEntries.begin(); it != indexedEntries.end(); ++it) {
        SavedServerEntry entry;
        entry.native=it->second.native;
        entry.address = sanitizeAddress(it->second.address);
        if (entry.address.empty()) {
            continue;
        }

        entry.port = sanitizePort(it->second.port);
        entry.name = sanitizeName(it->second.name, entry.address);
        entries.push_back(entry);
    }

    return entries;
}

bool SavedServerList::save(const std::vector<SavedServerEntry>& entries) {
    std::vector<std::string> preservedLines;

    FILE* input = std::fopen(getSettingsPath().c_str(), "r");
    if (input) {
        char lineBuffer[1024];
        while (std::fgets(lineBuffer, sizeof(lineBuffer), input)) {
            std::string line = stripNewlines(lineBuffer);
            if (line.empty()) {
                continue;
            }

            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                int index = -1;
                std::string field;
                if (parseSavedServerKey(line.substr(0, colon), index, field)) {
                    continue;
                }
            }

            preservedLines.push_back(line);
        }
        std::fclose(input);
    }

    FILE* output = std::fopen(getSettingsPath().c_str(), "w");
    if (!output) {
        return false;
    }

    for (size_t i = 0; i < preservedLines.size(); ++i) {
        std::fprintf(output, "%s\n", preservedLines[i].c_str());
    }

    for (size_t i = 0; i < entries.size(); ++i) {
        const SavedServerEntry& entry = entries[i];
        std::string address = sanitizeAddress(entry.address);
        if (address.empty()) {
            continue;
        }

        int port = sanitizePort(entry.port);
        std::string name = sanitizeName(entry.name, address);
        std::fprintf(output, "%s%u_name:%s\n", kSavedServerPrefix, (unsigned)i, name.c_str());
        std::fprintf(output, "%s%u_address:%s\n", kSavedServerPrefix, (unsigned)i, address.c_str());
        std::fprintf(output, "%s%u_port:%d\n", kSavedServerPrefix, (unsigned)i, port);
        std::fprintf(output,"%s%u_protocol:%s\n",kSavedServerPrefix,(unsigned)i,entry.native?"tcpe":"java");
    }

    std::fclose(output);
    return true;
}

bool SavedServerList::addOrUpdate(const SavedServerEntry& entry) {
    std::string address = sanitizeAddress(entry.address);
    if (address.empty()) {
        return false;
    }

    int port = sanitizePort(entry.port);
    std::vector<SavedServerEntry> entries = load();
    std::string name = sanitizeName(entry.name, address);

    for (size_t i = 0; i < entries.size(); ++i) {
        if (sanitizeAddress(entries[i].address) == address && sanitizePort(entries[i].port) == port && entries[i].native==entry.native) {
            entries[i].name = name;
            entries[i].address = address;
            entries[i].port = port;
            return save(entries);
        }
    }

    SavedServerEntry cleaned;
    cleaned.native=entry.native;
    cleaned.name = name;
    cleaned.address = address;
    cleaned.port = port;
    entries.push_back(cleaned);
    return save(entries);
}

bool SavedServerList::remove(const std::string& address, int port, bool native) {
    std::string cleanedAddress = sanitizeAddress(address);
    if (cleanedAddress.empty()) {
        return false;
    }

    int cleanedPort = sanitizePort(port);
    std::vector<SavedServerEntry> entries = load();

    for (std::vector<SavedServerEntry>::iterator it = entries.begin(); it != entries.end(); ++it) {
        if (sanitizeAddress(it->address) == cleanedAddress && sanitizePort(it->port) == cleanedPort && it->native==native) {
            entries.erase(it);
            return save(entries);
        }
    }

    return false;
}
