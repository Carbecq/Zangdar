#include <iostream>
#include <sstream>
#include <fstream>
#include "Tunable.h"

namespace Tunable
{

//==================================================
//! \brief  Constructeur d'un paramètre tunable
//--------------------------------------------------
TunableParam::TunableParam(const std::string& _name, int _value, int _min, int _max)
    : name(_name),
      value(_value),
      min(_min),
      max(_max)
{
    step = std::max(1, (_max - _min)/20);

    if (verifParam(_name, _value, _min, _max, step) == false)
        return;

    params.push_back(this);
}

bool verifParam(const std::string &name, int value, int min, int max, int step)
{
    int range = max - min;

    if (value < min)
    {
            std::cerr << "parameter " << name << " : value < min " << std::endl;
            return false;
    }
    else if (value > max)
    {
            std::cerr << "parameter " << name << " : value > max " << std::endl;
            return false;
    }
    else if (min >= max)
    {
            std::cerr << "parameter " << name << " :  min >= max " << std::endl;
            return false;
    }
    else if (min + step > max)
    {
            std::cerr << "parameter " << name << " : min+step > max " << std::endl;
            return false;
    }
    else if (step < 0.5)
    {
            std::cerr << "parameter " << name << " : step < 0.5 " << std::endl;
            return false;
    }
    // Grille trop grossière (moins de 5 valeurs possibles)
    else if (range / step < 5)
    {
        std::cerr << "parameter " << name << " : Grille trop grossière (moins de 5 valeurs possibles)" << std::endl;
        return false;
    }
    else if (value - min < step)
    {
        std::cerr << "parameter " << name << " : Valeur à moins d'un step du bord min" << std::endl;
        return false;
    }
    else if (max - value < step)
    {
        std::cerr << "parameter " << name << " : Valeur à moins d'un step du bord max" << std::endl;
        return false;
    }

    return true;
}

//==================================================
//! \brief  Modifie la valeur d'un paramètre par son nom
//--------------------------------------------------
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
    json.open("spsa_params.json");

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

//==================================================
//  Ecriture des paramètres au format SPSA
//  Format OpenBench : name, int, value, min, max, c_end, r_end
//--------------------------------------------------
std::string paramsToSpsa()
{
    std::ostringstream ss;

    for (auto param : params)
    {
        // c_end : perturbation size, typiquement step ou (max-min)/20
        double c_end = std::max(static_cast<double>(param->step), (param->max - param->min) / 20.0);
        // r_end : learning rate
        double r_end = 0.002;

        ss << param->name
           << ", " << "int"
           << ", " << double(param->value)
           << ", " << double(param->min)
           << ", " << double(param->max)
           << ", " << c_end
           << ", " << r_end
           << "\n";
    }

    return ss.str();}

} // namespace Tunable
