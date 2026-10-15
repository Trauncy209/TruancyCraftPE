#include "ChatScreen.h"
#include "../../NativeWeatherCommand.h"
#include "DialogDefinitions.h"
#include "../Gui.h"
#include "../../Minecraft.h"
#include "../../player/LocalPlayer.h"
#include "../../../AppPlatform.h"
#include "../../../network/RakNetInstance.h"
#include "../../../network/java/JavaGameSession.h"
#include "../../../network/ServerSideNetworkHandler.h"
#include "../../../network/packet/ChatPacket.h"

void ChatScreen::init() {
    minecraft->platform()->createUserInput(DialogDefinitions::DIALOG_NEW_CHAT_MESSAGE);
}

void ChatScreen::render(int xm, int ym, float a)
{
    (void)xm;
    (void)ym;
    (void)a;

    int status = minecraft->platform()->getUserInputStatus();
    if (status < 0)
        return;

    if (status == 1) {
        std::vector<std::string> v = minecraft->platform()->getUserInput();
        if (!v.empty() && !v[0].empty()) {
            std::string msg = std::string("<") + minecraft->player->name + "> " + v[0];
            if (minecraft->javaSession) {
                minecraft->javaSession->connection.chat(v[0]);
            } else if (NativeWeatherCommand::dispatch(*minecraft,v[0])) {
                // Same authority/usage behavior as the native console.
            } else if (minecraft->netCallback && minecraft->raknetInstance->isServer()) {
                static_cast<ServerSideNetworkHandler*>(minecraft->netCallback)->displayGameMessage(msg);
            } else if (minecraft->netCallback) {
                ChatPacket chatPkt(msg);
                minecraft->raknetInstance->send(chatPkt);
            } else {
                minecraft->gui.addMessage(msg);
            }
        }
    }

    minecraft->setScreen(NULL);
}
