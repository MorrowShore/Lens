#pragma once

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

// https://docs.microsoft.com/ru-ru/windows/win32/menurc/versioninfo-resource?redirectedfrom=MSDN

#define APP_INFO_PRODUCTVERSION_MAJOR 0
#define APP_INFO_PRODUCTVERSION_MINOR 31
#define APP_INFO_PRODUCTVERSION_PATCH 1

#define APP_INFO_PRODUCTVERSION         APP_INFO_PRODUCTVERSION_MAJOR, APP_INFO_PRODUCTVERSION_MINOR, APP_INFO_PRODUCTVERSION_PATCH, 0
#define APP_INFO_PRODUCTVERSION_STR     STR(APP_INFO_PRODUCTVERSION_MAJOR) "." STR(APP_INFO_PRODUCTVERSION_MINOR) "." STR(APP_INFO_PRODUCTVERSION_PATCH)

#define APP_INFO_FILEVERSION            APP_INFO_PRODUCTVERSION
#define APP_INFO_FILEVERSION_STR        APP_INFO_PRODUCTVERSION_STR

#define APP_INFO_PRODUCTNAME_STR            "AxelChat"
#define APP_INFO_COMPANYNAME_STR            "AXEL_K, 3dproger"
#define APP_INFO_EMAIL_STR                  "axelchatdev@gmail.com"
#define APP_INFO_LEGALCOPYRIGHT_YEARS_STR   "2020-2023"
#define APP_INFO_LEGALCOPYRIGHT_STR         "\251 " APP_INFO_LEGALCOPYRIGHT_YEARS_STR " " APP_INFO_COMPANYNAME_STR
#define APP_INFO_LEGALCOPYRIGHT_STR_U       "\u00A9 " APP_INFO_LEGALCOPYRIGHT_YEARS_STR " " APP_INFO_COMPANYNAME_STR
#define APP_INFO_LEGALTRADEMARKS1_STR       "All Rights Reserved"
#define APP_INFO_LEGALTRADEMARKS2_STR       APP_INFO_LEGALTRADEMARKS1_STR
#define APP_INFO_ORIGINALFILENAME_STR       "AxelChat.exe"
#define APP_INFO_FILEDESCRIPTION_STR        APP_INFO_PRODUCTNAME_STR
#define APP_INFO_INTERNALNAME_STR           APP_INFO_PRODUCTNAME_STR

#define APP_INFO_COMPANYDOMAIN_STR          "https://3dproger.github.io/AxelChat/"
#define APP_INFO_SITE_URL_STR               "https://3dproger.github.io/AxelChat/"
#define APP_INFO_SUPPORT_URL_STR            "https://3dproger.github.io/AxelChat/sponsor"
