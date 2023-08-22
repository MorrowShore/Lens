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
    Odysee,
    Rumble,
    VkPlayLive,
    VkVideo,
    Wasd,
    Telegram,
    Discord,

    DonationAlerts,
    DonatePayRu,
};

}
