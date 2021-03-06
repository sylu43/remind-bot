#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <cstdio>
#include <array>
#include <regex>
#include <algorithm>
#include <sqlite3.h>

#include "parser.h"
#include "db.h"

using namespace std;

extern uint64_t task_num;
extern sqlite3 *db;

int parse_remind_command(string s, int64_t group_id){
    smatch match;
    string msg;
    int freq, rc;
    int including = -1;
    string inclusions("");
    vector<array<int, 2>> periods;
    regex rgx("^/remind\\s+\"(.+)\"(\\s+[0-9]+)?(\\s+[ei]\\s+(\\d{1,2}(:\\d{2})?~\\d{1,2}(:\\d{2})?)(,\\s*\\d{1,2}(:\\d{2})?~\\d{1,2}(:\\d{2})?)*)?\\s*$");
    if(regex_search(s, match, rgx)){
        // message
        msg = string(match[1]);

        // frequency
        istringstream(match[2]) >> freq;
        if(freq > MINUTES_PER_DAY){
            return -4;
        }

        // include / exclude
        if(match[3].length() != 0){
            inclusions = string(match[3]);
            int pos = inclusions.find_first_of("ei");
            including = (inclusions[pos] == 'i' ? 1 : 0);
            inclusions[pos] = ' ';

            if(parse_remind_command_periods(inclusions, &periods) == -1){
                return -2;
            }
            if(remind_command_periods_check(periods) == -1){
                return -2;
            }
            if(including){
                periods = reverse_remind_command_periods(periods);
            }
            if(frequency_check(periods, freq) == -1){
                return -4;
            }
        }
        if(add_task_from_command(task_num++, group_id, msg, freq, including, periods) == -1){
            return -3;
        }
        return 0;
    } else {
        return -1;
    }
}

int parse_delete_command(string s){
    smatch match;
    int index;
    regex rgx("^/delete\\s+(\\d+)\\s*$");
    if(regex_search(s, match, rgx)){
        istringstream(match[1]) >> index;
    } else {
        index = -1;
    }
    return index;
}

int parse_remind_command_time(string s){
    stringstream ss(s);
    string tok;
    int hour, min = 0;
    getline(ss, tok, ':');
    hour = stoi(tok);
    if(getline(ss, tok, ':')){
        min = stoi(tok);
    }
    if(hour >23 || min > 59){
        return -1;
    }
    return hour * 60 + min;
}

int parse_remind_command_period(string s, array<int, 2> *period){
    stringstream ss(s);
    string tok;
    int start, end;
    getline(ss, tok, '~');
    start = parse_remind_command_time(tok);
    getline(ss, tok, '~');
    end = parse_remind_command_time(tok);
    if(start < 0 || end < 0){
        return -1;
    }
    *period = {start, end - start - 1};
    return 0;
}

int parse_remind_command_periods(string s, vector<array<int, 2>> *periods){
    string tok;
    stringstream ss(s);
    array<int, 2> period;
    while(getline(ss, tok, ',')){
        if(parse_remind_command_period(tok, &period) == -1){
            return -1;
        }
        (*periods).push_back(period);
    }

    // sort result
    sort((*periods).begin(), (*periods).end(), [](const array<int, 2>& lhs, const array<int, 2>& rhs){
            return lhs[0] < rhs[0];
        }
    );
    
    return 0;
}

int remind_command_periods_check(vector<array<int, 2>> periods){
    for(unsigned int i = 0; i < periods.size() - 1; i++){
        for(unsigned int j = i + 1; j < periods.size(); j++){
            if(periods[i][0] > periods[j][0] && periods[i][0] < (periods[j][0] + periods[j][1])){
                return -1;
            }
            if((periods[i][0] + periods[i][1]) > periods[j][0] && (periods[i][0] + periods[i][1]) < (periods[j][0] + periods[j][1])){
                return -1;
            }
        }
    }
    return 0;
}

int frequency_check(vector<array<int, 2>> periods, int  freq){
    int total_period = 0;
    for(auto period : periods){
        total_period += (period[1] + 1);
    }
    if(freq > MINUTES_PER_DAY - total_period){
        return -1;
    }
    return 0;
}

vector<array<int, 2>> reverse_remind_command_periods(vector<array<int, 2>> periods){
    vector<array<int, 2>> real_periods;
    int start = 0;
    for(auto period : periods){
        if(!period[0]){
            start = period[1] + 1;
            continue;
        }
        real_periods.push_back({start, period[0] - start - 1});
        start = period[0] + period[1] + 1;
    }
    if(start != MINUTES_PER_DAY - 1){
        real_periods.push_back({start, MINUTES_PER_DAY - start - 1});
    }
    return real_periods;
}

string stringify_periods(vector<array<int, 2>> periods){
    string s;
    for(auto period : periods){
        s += to_string(period[0]);
        s += "+";
        s += to_string(period[1]);
        s += ";";
    }
    s.back() = '\0';
    return s;
}

vector<array<int, 2>> unstringify_periods(string periods_str){
    int start;
    string tok1, tok2;
    stringstream ss1(periods_str);
    vector<array<int, 2>> vec;
    while(getline(ss1, tok1, ';')){
        stringstream ss2(tok1);
        getline(ss2, tok2, '+');
        start = stoi(tok2);
        getline(ss2, tok2, '+');
        vec.push_back({start, stoi(tok2)});
    }
    return vec;
}

void avoid_tag(string *s){
    regex re("[\\s=\\+\\-\\*/~`!@#$%\\^&\\(\\)\\[\\]{}|;:'\"\\\\<>,\\.]@([\\da-zA-Z_]{5,32}\\s)");
    *s = regex_replace(*s, re, " @ $1");
}
