#include <iostream>
#include <cstdio>
#include <string>
#include <cstring>
#include <array>
#include <vector>
#include <sqlite3.h>
#include <random>
#include "db.h"
#include "parser.h"

using namespace std;

extern sqlite3 *db;

static int check_table_empty_callback(void *data, int argc, char **argv, char **cols){
    int *result = (int *)data;
    if(argc == 1){
        (*result) = strtoll(argv[0], NULL, 0);
    } else {
        (*result) = 0;
    }
    return 0;
}

int check_table_empty(){
    char sql[512] = "select count(task_id) from " ALLTIME_TABLE_NAME;
    int result;
    sqlite3_exec(db, sql, check_table_empty_callback, &result, NULL);
    return result;
}

static int get_task_num_callback(void *data, int argc, char **argv, char **cols){
    int64_t *result = (int64_t *)data;
    if(argc == 1){
        (*result) = strtoll(argv[0], NULL, 0);
    } else {
        (*result) = 0;
    }
    return 0;
}

int64_t get_task_num(){
    sqlite3_stmt *stmt;
    int rc;
    int64_t result;
    rc = sqlite3_prepare_v2(db, "select max(task_id) from " ALLTIME_TABLE_NAME, -1, &stmt, NULL);
    if(rc != SQLITE_OK){
        return -1;
    }
    rc = sqlite3_step(stmt);
    if(rc != SQLITE_ROW && rc != SQLITE_DONE){
        result = -1;
    } else if(rc == SQLITE_DONE){
        result = -1;
    } else if(sqlite3_column_type(stmt, 0) == SQLITE_NULL){
        result = 0;
    } else if(rc == SQLITE_ROW){
        result = sqlite3_column_int64(stmt, 0);
    } else {
        result = -1;
    }
    sqlite3_finalize(stmt);
    return result;
}

int check_table_or_view(string table, FORM form){
    sqlite3_stmt *stmt;
    int rc;
    rc = sqlite3_prepare_v2(db, "select 1 from sqlite_master where type=? and name=?", -1, &stmt, NULL);
    if(rc != SQLITE_OK){
        return -1;
    }
    sqlite3_bind_text(stmt, 1, (form == TABLE) ? "table" : "view", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, table.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    if(rc == SQLITE_ROW){
        rc = 1;
    } else if(rc == SQLITE_DONE){
        rc = 0;
    } else {
        rc = -1;
    }
    sqlite3_finalize(stmt);
    return rc;
}

int set_tables(){
    int rc;
    // make all time table if it doesn't exist
    rc = check_table_or_view(ALLTIME_TABLE_NAME, TABLE);
    if(rc == 0){
        if(sqlite3_exec(db, "create table " ALLTIME_TABLE_NAME "(task_id int, group_id int, msg text, frequency int, periods text)", NULL, 0, NULL) != SQLITE_OK){
            cout << "create table err\n";
            return -1;
        }
    } else if (rc == -1){
        return -1;
    }

    // make daily table if it doesn't exist
    rc = check_table_or_view(DAILY_TABLE_NAME, TABLE);
    if(rc == 0){
        if(sqlite3_exec(db, "create table " DAILY_TABLE_NAME "(task_id int, time int)", NULL, 0, NULL) != SQLITE_OK){
            cout << "create table err\n";
            return -1;
        }
    } else if (rc == -1){
        return -1;
    }
    return 0;
}

int set_view(){
    // make whole day count view if not exist
    int rc = check_table_or_view(WHOLE_DAY_VIEW_NAME, VIEW);
    if(rc == 0){
        if(sqlite3_exec(db, "create view " WHOLE_DAY_VIEW_NAME " as select task_id, count(task_id) as task_count from daily group by task_id", NULL, 0, NULL) != SQLITE_OK){
            cout << "create view err\n";
            return -1;
        }
    } else if (rc == -1){
        return -1;
    }
    return 0;
}


int add_task_from_command(int task_num, int64_t group_id, string msg, int frequency, int including, vector<array<int, 2>> periods){
    char sql[512];
    string periods_str;

    if(including != -1){
        periods_str = stringify_periods(periods);
    }

    // add to all time DB first
    sprintf(sql, "insert into " ALLTIME_TABLE_NAME "(task_id, group_id, msg, frequency, periods) values (%d, %lld, '%s', %d, '%s')", task_num, group_id, msg.c_str(), frequency, periods_str.c_str());
    if(sqlite3_exec(db, sql, NULL, 0, NULL) != SQLITE_OK){
        return -1;
    }

    // calculate quota left for the remaining day
    time_t now = time(NULL);
    tm *tm_local = localtime(&now);
    int cur_min = tm_local->tm_min + tm_local->tm_hour * 60;
    //add to daily DB
    new_reminds(task_num, frequency, periods, cur_min);
    return 0;
}

int new_reminds(int task_num, int frequency, vector<array<int, 2>> periods, int cur_min){
    //calculate available time available
    int total_offset = 0;
    char sql[512];
    for(auto period : periods){
        total_offset += (period[1] + 1);
    }
    // add reminds
    int t;
    random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> distrib(0, MINUTES_PER_DAY - total_offset - 1);
    for(int i = 0; i < frequency; i++){
        t = distrib(gen);
        for(auto period : periods){
            if(t >= period[0]){
                t += (period[1] + 1);
            }
        }
        if(t <= cur_min){
            continue;
        }
        sprintf(sql, "insert into " DAILY_TABLE_NAME "(task_id, time) values (%d, %d)", task_num, t);
        if(sqlite3_exec(db, sql, NULL, 0, NULL) != SQLITE_OK){
            return -1;
        }
    }
    return 0;
}

int list_index;
static int list_callback(void *data, int argc, char **argv, char **cols){
    string *result = (string *)data;
    //list_index
    *result += to_string(list_index++);
    *result += ". ";
    //msg
    *result += argv[0];
    *result += "(";
    *result += argv[1] ? argv[1] : "0";
    *result += "/";
    *result += argv[2] ? argv[2] : "0";
    *result += ")\n";
    return 0;
}

string find_all_task_in_group(int64_t group_id, int cur_min){
    char sql[512], temp[512] = "";
    string s;
    int num, rc;
    
    //drop and make view for done reminds
    rc = check_table_or_view(DONE_VIEW_NAME, VIEW);
    if(rc == 1){
        sqlite3_exec(db, "drop view " DONE_VIEW_NAME, NULL, 0, NULL);
    }
    sprintf(sql, "create view " DONE_VIEW_NAME " as select task_id, count(task_id) as task_count from daily where time < %d group by task_id", cur_min);
    if(sqlite3_exec(db, sql, NULL, 0, NULL) != SQLITE_OK){
        cout << "create view err\n";
        return "err";
    }

    //get task list
    list_index = 1;
    sprintf(sql, "select " ALLTIME_TABLE_NAME ".msg, " DONE_VIEW_NAME ".task_count, " WHOLE_DAY_VIEW_NAME ".task_count from " ALLTIME_TABLE_NAME " LEFT JOIN " DONE_VIEW_NAME " on " ALLTIME_TABLE_NAME ".task_id = " DONE_VIEW_NAME ".task_id LEFT JOIN " WHOLE_DAY_VIEW_NAME " on " ALLTIME_TABLE_NAME ".task_id = " WHOLE_DAY_VIEW_NAME ".task_id where group_id = %lld", group_id);
    if(sqlite3_exec(db, sql, list_callback, &s, NULL) != SQLITE_OK){
        cout << "query err";
    }
    
    return (s.length() > 0) ? s : "No task!";
}

int prefetch_reminds(char* sql, vector<pair<int64_t, string>>* remind_buffer){
    vector<pair<int64_t, string>>* result= remind_buffer;
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if(rc != SQLITE_OK){
        return -1;
    }
    while((rc = sqlite3_step(stmt)) == SQLITE_ROW){
        (*result).push_back(make_pair((int64_t)sqlite3_column_int64(stmt, 0), (const char*)sqlite3_column_text(stmt, 1)));
    }
    if(rc != SQLITE_DONE){
        return -1;
    }
    sqlite3_finalize(stmt);
    return 0;
}

int delete_task(int64_t group_id, int task_index){
    char sql[512];
    int task_id, rc;
    sqlite3_stmt *stmt;

    //get task id
    sprintf(sql, "select task_id from " ALLTIME_TABLE_NAME " where group_id = %lld order by task_id", group_id, (task_index - 1));
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if(rc != SQLITE_OK){
        return -1;
    }
    for(int i = 0; i < task_index; i++){
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_ROW && rc != SQLITE_DONE){
            task_id = -1;
            break;
        } else if(rc == SQLITE_DONE){
            task_id = -2;
            break;
        } else if(rc == SQLITE_ROW){
            task_id = sqlite3_column_int(stmt, 0);
        } else {
            task_id = -1;
            break;
        }
    }
    sqlite3_finalize(stmt);
    if(task_id < 0){
        return task_id;
    }

    //deletion
    sprintf(sql, "delete from " DAILY_TABLE_NAME " where task_id = %d", task_id);
    rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
    if(rc){
        return -1;
    }
    sprintf(sql, "delete from " ALLTIME_TABLE_NAME " where task_id = %d", task_id);
    rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
    if(rc){
        return -1;
    }
    return 0;
}
