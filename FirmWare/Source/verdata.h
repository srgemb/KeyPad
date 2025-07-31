
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
    #define FW_VERSION_BUILD   85
    #define FW_VERSION_RC      'D'

    #define FW_TIME_HOUR       20
    #define FW_TIME_MINUTES    6
    #define FW_TIME_SECONDS    41

    #define FW_DATE_DAY        31
    #define FW_DATE_MONTH      7
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
    #define FW_VERSION_BUILD   49
    #define FW_VERSION_RC      'R'

    #define FW_TIME_HOUR       20
    #define FW_TIME_MINUTES    6
    #define FW_TIME_SECONDS    42

    #define FW_DATE_DAY        31
    #define FW_DATE_MONTH      7
    #define FW_DATE_YEAR       2025
#endif

#endif

