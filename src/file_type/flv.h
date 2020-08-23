/*
 * This file is part of the StegX project.
 * Copyright (C) 2018  StegX Team
 * 
 * StegX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/** 
 * @file flv.h
 * @brief Fichier FLV.
 * @details Contient les informations (fonctions, types, structures) relatives
 * aux fichiers au format FLV.
 */

#ifndef FLV_H
#define FLV_H

#include <stdio.h>
#include <stdint.h>

#include "common.h"

/** valeur d'un tag de type video  */
#define VIDEO_TAG 9

/** valeur d'un tag de type audio  */
#define AUDIO_TAG 8

/** valeur d'un tag de type metadata en entier non signé */
#define METATAG 18

/** valeur d'un tag de type script data en entier non signé  */
#define SCRIPT_DATA_TAG 24

/**
 * @brief Structure du format FLV.
 * @author Claire Baskevitch et Tristan Bessac
 */
struct flv {
    uint32_t nb_video_tag;      /*!< Nombre de chunks video dans le fichier. */
    uint32_t nb_metadata_tag;   /*!< Nombre de chunks metadonnees dans le fichier. */
    uint32_t file_size;         /*!< Taille du fichier en octets. */
};

/** Type du format FLV. */
typedef struct flv flv_s;

/**
 * @brief Test si le fichier est un fichier FLV.
 * @param file Fichier à tester.
 * @req Le pointeur ne doit pas être null et le fichier ouvert en lecture.
 * @return \r{FLV}, \r{UNKNOWN} ou -1 en cas d'erreur. 
 * @author Claire Baskevitch et Tristan Bessac
 */
type_e stegx_test_file_flv(FILE * file);

/** 
 * @brief Va inserer les donnees cachees en utilisant l'algorithme Metadata 
 * dans le format FLV. 
 * @param infos Structure représentant les informations concernant la dissimulation.
 * @return 0 si les données ont bien été inserées ; sinon 1 en cas d'erreur.
 * @author Claire Baskevitch et Tristan Bessac
 */
int insert_metadata_flv(info_s * infos);

/** 
 * @brief Va extraire les donnees cachees en utilisant l'algorithme Metadata
 * dans le formar FLV. 
 * @param infos Structure représentant les informations concernant l'extraction.
 * @return 0 si les données ont bien été extraites ; sinon 1 en cas d'erreur.
 * @author Claire Baskevitch et Tristan Bessac
 */
int extract_metadata_flv(info_s * infos);

#endif
