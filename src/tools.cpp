#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include "defines.h"

//======================================================================
//! \brief  Découpe une chaine en un vecteur de sous-chaines
//! \param[in]  s           chaine à découper
//! \param[in]  delimiter   séparateur
//! \return                 vecteur contenant les sous-chaines
//----------------------------------------------------------------------
std::vector<std::string> split(const std::string& s, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

//======================================
//! \brief Ecriture dans le fichier de log
//--------------------------------------
void printlog(const std::string& message)
{
    std::ofstream myfile;

    // Chemin relatif : le fichier est créé dans le répertoire courant.
    myfile.open("debug.txt", std::ios_base::app); // ajoute à la suite plutôt que d'écraser
    myfile << message << std::endl;
    myfile.close();
}

