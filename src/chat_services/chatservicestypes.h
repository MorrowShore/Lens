#pragma once

#include <QObject>

namespace AxelChat
{

enum class ServiceType
{
    Unknown = 0,
    Software = 1,

    YouTube,
    Twitch,
    Trovo,
    GoodGame,
    Kick,
    DLive,
    Rumble,
    BigoLive,
    //BigoLive,
    //TikTok,
    //FacebookLive,
    NimoTV,
    Odysee,
    VkPlayLive,
    VkVideo,
    Wasd,
    Rutube,

    Telegram,
    Discord,

    //StreamElements,
    //Streamlabs,
    DonationAlerts,
    DonatePayRu,
    //DonatePayEu,

    //Patreon,
    //Boosty,
};

}
