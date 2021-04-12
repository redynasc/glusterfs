/*
  Copyright (c) 2008-2012 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include "fs-cache.h"

#define Time_Hour  3600
#define Time_24Hour  (Time_Hour * 24)
#define Time_1Week   (Time_24Hour*7)


struct fsc_period {
    char p_type;
    int8_t week;
    int8_t hour;
    int8_t minute;
    int32_t sec;
};

typedef struct fsc_period fsc_period_t;

// int32_t fsc_zonediff_sec(){
//     // static int32_t iZoneDiffSec = -1;
//     // if (iZoneDiffSec == -1)
//     // {
//     //     time_t inow = time(NULL);
//     //     struct tm* local;
//     //     local = localtime(&inow);
//     //     time_t localSec = ::mktime(local);

//     //     struct tm* utc;
//     //     utc = gmtime(&inow);
//     //     time_t gmSec = ::mktime(utc);
//     //     iZoneDiffSec = (int)(localSec - gmSec);//已经包含夏令时逻辑
//     // }
//     // return iZoneDiffSec;
//     return 8*3600;
// }


time_t fsc_today_local_zero(time_t now){
    const int32_t zone_diff = 8*3600;
    time_t lastlt = 0;
    now += zone_diff;
    lastlt = now - now%Time_24Hour;//utc凌晨
    lastlt -= zone_diff; //本地凌晨
    return lastlt;
}

fsc_period_t
fsc_pasre_period(const char* period){
    size_t len = strlen(period);
    fsc_period_t fpt;
    fpt.p_type = 'U';
    if (len == 0) {
        return fpt;
    }
    fpt.p_type  = period[0];
    if ( fpt.p_type == 'P' ){
    	//"P60" 每隔固定时间执行
        fpt.sec = 0;
        if (len >= 2){
            fpt.sec = atoi(&period[1]);
        }
    }
    else if ( fpt.p_type == 'D' ){
        //"D02:00:00" 每天的某一时间执行
        fpt.hour = 0;
        if (len >= 3){
            fpt.hour = (int8_t)atoi(&period[1]);
        }
        fpt.minute = 0;
        if (len >= 6){
            fpt.minute = (int8_t)atoi(&period[4]);
        }
        fpt.sec = 0;
        if (len >= 9){
            fpt.sec = (int8_t)atoi(&period[7]);
        }
    }else if ( fpt.p_type == 'W' ){
        //"W01 02:00:00" 每周几的某一时间执行  todo
    }
    return fpt;
}

time_t
fsc_next_time(const char* period, struct timeval *now){
    time_t ret = 0;
    fsc_period_t fpt = fsc_pasre_period(period);
    if ( fpt.p_type == 'P' ){
        ret = now->tv_sec + fpt.sec;
    }
    else if ( fpt.p_type == 'D' ){
        ret = (time_t)fsc_today_local_zero(now->tv_sec) + fpt.hour*3600 + fpt.minute*60 + fpt.sec;
        if (ret < now->tv_sec){
              ret += 24*3600;
        }
    }else{
        //default or error
        ret = now->tv_sec + 12*3600;
    }
    return ret;
}

// int main(int argc, char *argv[]) {
//     if (argc < 1) {
//         printf("please input period\n");
//         return;
//     }
//     struct timeval now = {
//         0,
//     };
//     gettimeofday(&now, NULL);
//     printf("%d",(int)fsc_next_time(argv[1],&now));
// }