#ifndef UCI_H
#define UCI_H

class Uci;

#include <string>
#include "defines.h"

class Uci
{
public:
    Uci() = default;

    void run();
    void bench(int argCount, char* argValue[]);

private:
    void stop();
    void quit();
    void parse_go(std::istringstream& iss);
    void parse_options(std::istringstream& iss);
    void go_run(const std::string& abc, const std::string &fen, int dmax, int tmax, int nmax);
    void go_test(int dmax, int tmax);
    void go_tactics(const std::string& line, int dmax, int tmax, U64& total_nodes, U64& total_time, int &total_depths, int &total_bm, int &total_am, int &total_ko);

};

#endif // UCI_H
