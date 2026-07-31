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

//======================================================================
//! \brief  Clé de position d'une ligne EPD/FEN : les 4 premiers champs
//!         (placement, trait, roque, e.p.), sans les annotations (bm/am/id...)
//!         ni les éventuels compteurs demi-coups/coups — ces derniers ne
//!         changent ni les coups légaux ni le perft.
//!         Sert à détecter les positions en double dans une suite de tests,
//!         via un hash set : O(1) par position au lieu d'un std::find linéaire.
//!
//! \param[in]  epd_line  ligne EPD/FEN à analyser
//!
//! \return                 clé (4 premiers champs concaténés)
//----------------------------------------------------------------------
std::string position_key(const std::string& epd_line)
{
    std::istringstream iss(epd_line);
    std::string tok, key;
    for (int i = 0; i < 4 && (iss >> tok); ++i)
        key += (i ? " " : "") + tok;
    return key;
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

