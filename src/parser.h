#ifndef PARSER_H
#define PARSER_H
#include <array>
#include <vector>

#define MINUTES_PER_DAY     (60 * 24)

using namespace std;

int parse_remind_command(string s, int64_t group_id);
int parse_delete_command(string s, int64_t group_id);
int parse_remind_command_time(string s);
array<int, 2> parse_remind_command_period(string s);
vector<array<int, 2>> parse_remind_command_periods(string s);
int remind_command_periods_check(vector<array<int, 2>> periods);
vector<array<int, 2>> reverse_remind_command_periods(vector<array<int, 2>> periods);

#endif
