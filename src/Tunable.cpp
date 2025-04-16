#include <iostream>
#include <sstream>
#include <fstream>
#include "Tunable.h"

namespace Tunable
{

TunableParam::TunableParam(const std::string& _name, int _value, int _min, int _max, int _step)
    : name(_name),
      value(_value),
      min(_min),
      max(_max),
      step(_step)
{
    if (value < min || value > max)
    {
        std::cerr << "info string Value out of range for parameter " << name << std::endl;
        return;
    }

    params.push_back(this);
}

void setParam(const std::string &name, int value)
{
    for (auto param : params)
    {
        if (param->name == name)
        {
            if (value < param->min || value > param->max)
            {
                std::cerr << "info string Value out of range for parameter " << name << std::endl;
                return;
            }

            param->value = value;
            return;
        }
    }
}

//==================================================
//  Ecriture des paramètres au format "Option UCI"
//--------------------------------------------------
std::string paramsToUci()
{
    std::ostringstream ss;

    for (auto param : params)
        ss << "option name " << param->name
           << " type spin default " << param->value
           << " min " << param->min
           << " max " << param->max << std::endl;

    return ss.str();
}

//==================================================
//  Ecriture des paramètres au format json
//  Pour être utilisé dans le fichier config.json
//--------------------------------------------------
void paramsToJSON()
{
    std::ofstream json;
    json.open("config.json");

    json << "{\n";

    bool first = true;
    for (auto param : params)
    {
        if (!first)
            json << ",\n";

        json << "  \""                << param->name << "\": {\n";
        json << "    \"value\": "     << param->value << ",\n";
        json << "    \"min_value\": " << param->min << ",\n";
        json << "    \"max_value\": " << param->max << ",\n";
        json << "    \"step\": "      << param->step << "\n";
        json << "  }";

        first = false;
    };

    json << "\n}" << std::endl;
    json.close();
}


} // namespace Tunable
