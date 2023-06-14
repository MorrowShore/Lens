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
    VkPlayLive,
    VkVideo,
    Wasd,
    Telegram,
    Discord,
};

}
