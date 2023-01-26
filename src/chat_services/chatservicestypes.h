#pragma once

#include <QObject>

namespace AxelChat
{

enum class ServiceType
{
    Unknown = -1,

    Software = 10,

    YouTube = 100,
    Twitch = 101,
    Trovo = 102,
    GoodGame = 103,
    VkPlayLive = 104,
    Telegram = 105,
};

}
