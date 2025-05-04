
#ifndef __VERDATA_H
#define __VERDATA_H

#ifdef FW_TEST
    #define FW_VERSION_MAJOR   0
    #define FW_VERSION_MINOR   0
    #define FW_VERSION_BUILD   0
    #define FW_VERSION_RC      'T'

    #define FW_TIME_HOUR       14
    #define FW_TIME_MINUTES    43
    #define FW_TIME_SECONDS    28

    #define FW_DATE_DAY        21
    #define FW_DATE_MONTH      1
    #define FW_DATE_YEAR       2025
#endif

#ifdef FW_DEBUG
    #define FW_VERSION_MAJOR   0
    #define FW_VERSION_MINOR   0
    #define FW_VERSION_BUILD   44
    #define FW_VERSION_RC      'D'

    #define FW_TIME_HOUR       18
    #define FW_TIME_MINUTES    1
    #define FW_TIME_SECONDS    18

    #define FW_DATE_DAY        3
    #define FW_DATE_MONTH      5
    #define FW_DATE_YEAR       2025
#endif

#ifdef FW_PRE_RELEASE
    #define FW_VERSION_MAJOR   0
    #define FW_VERSION_MINOR   0
    #define FW_VERSION_BUILD   0
    #define FW_VERSION_RC      'P'

    #define FW_TIME_HOUR       14
    #define FW_TIME_MINUTES    43
    #define FW_TIME_SECONDS    32

    #define FW_DATE_DAY        21
    #define FW_DATE_MONTH      1
    #define FW_DATE_YEAR       2025
#endif

#ifdef FW_RELEASE
    #define FW_VERSION_MAJOR   0
    #define FW_VERSION_MINOR   0
    #define FW_VERSION_BUILD   18
    #define FW_VERSION_RC      'R'

    #define FW_TIME_HOUR       17
    #define FW_TIME_MINUTES    31
    #define FW_TIME_SECONDS    54

    #define FW_DATE_DAY        3
    #define FW_DATE_MONTH      5
    #define FW_DATE_YEAR       2025
#endif

#endif

