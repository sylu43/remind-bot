#ifndef DB_H
#define DB_H
#include <sqlite3.h>

#define ALLTIME_TABLE_NAME  "all_time"
#define DAILY_TABLE_NAME    "daily"
#define WHOLE_DAY_VIEW_NAME "whole_day_v"
#define DONE_VIEW_NAME      "done_v"

using namespace std;

enum FORM{TABLE, VIEW};

typedef struct task_info_s{
    int task_id;
    int frequency;
    string periods_str;
} task_info_t;

int check_table_empty();
int64_t get_task_num();
int check_table_or_view(string table, FORM form);
int set_tables();
int set_view();
int add_task_from_command(int task_id, int64_t group_id, string msg, int frequency, int including, vector<array<int, 2>> inclusions);
int add_task_daily();
int new_reminds(int task_id, int frequency, vector<array<int, 2>> periods, int cur_min);
string find_all_task_in_group(int64_t group_id, int cur_min);
int fetch_reminds(char* sql, vector<pair<int64_t, string>>* remind_buffer);
int delete_task(int64_t group_id, int task_index);

#endif
