#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <vector>
#include <sstream>
#include <regex>
#include <chrono>
#include <thread>
#include <sqlite3.h>
#include <sys/types.h>

#include <tgbot/tgbot.h>
#include "parser.h"
#include "db.h"

#define DB_PATH "reminder.db"

using namespace std;
using namespace TgBot;

uint64_t task_num;
sqlite3 *db;

void remind_thread_f(Bot *bot){
    time_t now = time(NULL);
    tm *tm_local = localtime(&now);
    int cur_min = tm_local->tm_hour * 60 + tm_local->tm_min;
    char sql[512];
    vector<pair<int64_t, string>> remind_buffer;
    
    while(1){
        now = time(NULL);
        tm_local = localtime(&now);
        cur_min = tm_local->tm_hour * 60 + tm_local->tm_min;
        //set daily reminds
        if(!cur_min){
            add_task_daily();
        }
        //fetch messages
        sprintf(sql, "select all_time.group_id, all_time.msg from all_time LEFT JOIN daily on all_time.task_id = daily.task_id where time=%d", cur_min);
        fetch_reminds(sql, &remind_buffer);
        for(auto remind : remind_buffer){
            (*bot).getApi().sendMessage(remind.first, remind.second);
        }
        remind_buffer.clear();

        //sleep for next round and send
        now = time(NULL);
        tm_local = localtime(&now);
        cur_min = tm_local->tm_hour * 60 + tm_local->tm_min;
        this_thread::sleep_for(chrono::seconds(60 - tm_local->tm_sec));
    }
}

int main() {
    string token(getenv("TOKEN"));
    printf("Token: %s\n", token.c_str());

    if(sqlite3_open(DB_PATH, &db)){
        cout << "Opening DB errer!\n";
        sqlite3_close(db);
    }

    if(set_tables() == -1){
        return -1;
    }

    if(set_view() == -1){
        return -1;
    }

    task_num = get_task_num() + 1;

    Bot bot(token);
    bot.getEvents().onAnyMessage([&bot](Message::Ptr message) {
        if(message->text.find("不要") != string::npos || message->text.find("鼻藥") != string::npos || message->text.find("鼻要") != string::npos){
            bot.getApi().sendSticker(message->chat->id, "CAACAgUAAx0CacyfrAACAxdh6AHKQ_9ITQ6HW9uVkT9uf2NZygAC5gIAArDPuVV6z6He8xoIDCME", message->messageId);
        }
        return;
    });
    bot.getEvents().onCommand("delete", [&bot](Message::Ptr message) {
        int task_index, rc;
        int64_t group_id = message->chat->id;
        if((task_index = parse_delete_command(message->text)) == -1){
            bot.getApi().sendMessage(group_id, "Bad command");
            return;
        }
        rc = delete_task(group_id, task_index);
        if(rc == -1){
            bot.getApi().sendMessage(group_id, "Internal error");
            return;
        } else if (rc == -2){
            bot.getApi().sendMessage(group_id, "Index out of range");
            return;
        }
        bot.getApi().sendMessage(group_id, "Task deleted!");
        return;
    });
    bot.getEvents().onCommand("bonk", [&bot](Message::Ptr message) {
        srand(time(NULL));
        bot.getApi().sendSticker(message->chat->id, "CAACAgUAAx0CacyfrAACAxdh6AHKQ_9ITQ6HW9uVkT9uf2NZygAC5gIAArDPuVV6z6He8xoIDCME", message->messageId - (int)(100.0 * rand() / RAND_MAX));
        return;
    });
    bot.getEvents().onCommand("list", [&bot](Message::Ptr message) {
        time_t now = time(NULL);
        tm *tm_local = localtime(&now);
        int cur_min = tm_local->tm_hour * 60 + tm_local->tm_min;
        int64_t group_id = message->chat->id;
        string s = find_all_task_in_group(group_id, cur_min);
        avoid_tag(&s);
        bot.getApi().sendMessage(group_id, s);
    });
    bot.getEvents().onCommand("remind", [&bot](Message::Ptr message) {
        int64_t group_id = message->chat->id;
        int rc = parse_remind_command(message->text, group_id); 
        if(rc == 0){
            bot.getApi().sendMessage(group_id, "Task set!");
        } else if(rc == -1){
            bot.getApi().sendMessage(group_id, "Bad command");
        } else if(rc == -2){
            bot.getApi().sendMessage(group_id, "Bad time periods");
        } else if (rc == -3){
            bot.getApi().sendMessage(group_id, "Internal error!");
        }
    });
    signal(SIGINT, [](int s) {
        printf("SIGINT got\n");
        exit(0);
    });

    thread remind_thread(remind_thread_f, &bot);

    try {
        printf("Bot username: %s\n", bot.getApi().getMe()->username.c_str());
        bot.getApi().deleteWebhook();

        TgLongPoll longPoll(bot);
        while (true) {
            printf("Long poll started\n");
            longPoll.start();
        }
    } catch (exception& e) {
        printf("error: %s\n", e.what());
    }

    return 0;
}
