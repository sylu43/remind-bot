#include <tgbot/tgbot.h>

#include "remind.h"

using namespace std;
using namespace TgBot;

extern sqlite3 *db;

int remind_callback(Bot *bot, char* sql){
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if(rc != SQLITE_OK){
        return -1;
    }
    while((rc = sqlite3_step(stmt)) == SQLITE_ROW){
        (*bot).getApi().sendMessage((int64_t)sqlite3_column_int64(stmt, 0), (const char*)sqlite3_column_text(stmt, 1));
    }
    if(rc != SQLITE_DONE){
        return -1;
    }
    sqlite3_finalize(stmt);
    return 0;
}

void remind(Bot *bot){
    time_t now = time(NULL);
    tm *tm_local = localtime(&now);
    int cur_min = tm_local->tm_hour * 60 + tm_local->tm_min;
    char sql[512];
    
    while(1){
        //run tasks
        sprintf(sql, "select group_id, msg from daily where time=%d", cur_min);
        remind_callback(bot, sql);

        //sleep for next round
        now = time(NULL);
        tm_local = localtime(&now);
        cur_min = tm_local->tm_hour * 60 + tm_local->tm_min;
        this_thread::sleep_for(chrono::seconds(60 - tm_local->tm_sec));
    }
}

