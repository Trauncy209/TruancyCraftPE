#ifndef _MINECRAFT_NETWORK_RAKNETINSTANCE_H_
#define _MINECRAFT_NETWORK_RAKNETINSTANCE_H_

#include <vector>
#include <string>
#include "ClientProtocol.h"
#include <memory>
class NativeServerStatus;
#include "../raknet/RakNetTypes.h"
#include "../raknet/RakString.h"

namespace RakNet
{
	class RakPeerInterface;
}

class Packet;
class NetEventCallback;

typedef struct PingedCompatibleServer
{
	RakNet::RakString name;
	RakNet::SystemAddress address;
	RakNet::TimeMS pingTime;
	bool isSpecial;
} PingedCompatibleServer;
typedef std::vector<PingedCompatibleServer> ServerList;

class IRakNetInstance
{
public:
    virtual ~IRakNetInstance() {}

	virtual bool host(const std::string& localName, int port, int maxConnections = 4) { return false; }
	virtual bool connect(const char* host, int port) { return false; }
	virtual void setIsLoggedIn(bool status) {}
	virtual void setClientProtocol(ClientProtocol protocol) {}
	virtual ClientProtocol getClientProtocol() const { return CLIENT_PROTOCOL_CLASSIC; }

	virtual void pingForHosts(int port) {}
	virtual bool pingForHost(const char* host, int port) { return false; }
	virtual void stopPingForHosts() {}
	virtual const ServerList& getServerList() { static ServerList l; return l; }
	virtual void clearServerList() {}

	virtual void disconnect() {}

	virtual void announceServer(const std::string& localName) {}

	virtual RakNet::RakPeerInterface* getPeer() { return NULL; }
	virtual bool isMyLocalGuid(const RakNet::RakNetGUID& guid) { return true; }

	virtual void runEvents(NetEventCallback* callback) {}

	virtual void send(Packet& packet) {}
	virtual void send(const RakNet::RakNetGUID& guid, Packet& packet) {}

	// @attn: Those delete the packet
	virtual void send(Packet* packet) {}
	virtual void send(const RakNet::RakNetGUID& guid, Packet* packet) {}

	virtual bool isServer() { return true; }
    virtual bool isProbablyBroken() { return false; }
	virtual void resetIsBroken() {}
};

class RakNetInstance: public IRakNetInstance
{
public:
    bool listingActive() const;

	RakNetInstance();
	virtual ~RakNetInstance();

	bool host(const std::string& localName, int port, int maxConnections = 4);
	bool connect(const char* host, int port);
	void setIsLoggedIn(bool status);
	void setClientProtocol(ClientProtocol protocol);
	ClientProtocol getClientProtocol() const;

	void pingForHosts(int basePort);
	bool pingForHost(const char* host, int port);
	void stopPingForHosts();
	const ServerList& getServerList();
	void clearServerList();

	void disconnect();

	void announceServer(const std::string& localName);

	RakNet::RakPeerInterface* getPeer();
	bool isMyLocalGuid(const RakNet::RakNetGUID& guid);

	void runEvents(NetEventCallback* callback);

	void send(Packet& packet);
	void send(const RakNet::RakNetGUID& guid, Packet& packet);

	// @attn: Those delete the packet
	void send(Packet* packet);
	void send(const RakNet::RakNetGUID& guid, Packet* packet);

	bool isServer() { return _isServer; }
    bool isProbablyBroken();
	void resetIsBroken();

#ifdef _DEBUG
	const char* getPacketName(int packetId);
#else
	const char* getPacketName(int packetId) { return ""; }
#endif

private:
    std::unique_ptr<NativeServerStatus> nativeStatus;int nativePort=0,nativeMaximum=0;
	int handleUnconnectedPong(const RakNet::RakString& data, const RakNet::Packet*, const char* appid, bool insertAtBeginning);

	RakNet::RakPeerInterface*	rakPeer;
	RakNet::RakNetGUID			serverGuid;

	ServerList		availableServers;
	bool			isPingingForServers;
	int				pingPort;
	RakNet::TimeMS	lastPingTime;

	bool _isServer;
	bool _isLoggedIn;
	ClientProtocol _clientProtocol;
};

#endif /*_MINECRAFT_NETWORK_RAKNETINSTANCE_H_*/
