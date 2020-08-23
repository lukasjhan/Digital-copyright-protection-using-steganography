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
 * @file stegx.h
 * @brief Fonctions d'utilisations de la bibliothèque StegX.
 * @details Fichier contenant des fonctions publiques destinées à être utilisées
 * par les applications utilisant la bibliothèque.
 */

#ifndef STEGX_H
#define STEGX_H

#include "stegx_common.h"
#include "stegx_errors.h"

/**
 * @brief Initialise la bibliothèque. 
 * @details Initialise la bibliothèque avec les choix de l'utilisateur en
 * remplissant les structures nécéssaire et en ouvrant les fichiers.
 * @sideeffect Initialise la structure \r{info_s} en fonction des choix de
 * l'utilisateur. Il restera les champs \r{info.hidden_length}, \r{info.algo}, 
 * \r{info.host.type}, \r{info.host.file_info} à initialiser.
 * @req La structure \r{stegx_choices_s} doit être initialisée comme indiquée
 * dans sa description.
 * @error \r{ERR_HOST} si une erreur survient pendant l'ouverture du fichier hôte.
 * @error \r{ERR_HIDDEN} si une erreur survient pendant l'ouverture du fichier à caché.
 * @error \r{ERR_RES_INSERT} si une erreur surivient pendant l'ouverture du
 * fichier résultat lors de l'insertion.
 * @error \r{ERR_RES_EXTRACT} si une erreur surivient pendant l'ouverture du
 * dossier résultat lors de l'extraction.
 * @error \r{ERR_PASSWD} si le mot de passe fourni est non-conforme.
 * @param choices Structure contenant les choix de l'utilisateur.
 * @return Pointeur sur la structure privée qui contient les informations pour
 * réaliser la dissimulation et l'extraction, sinon NULL sur une erreur et met à
 * jour la variable \r{stegx_errno}. 
 * @author Clément Caumes et Pierre Ayoub
 */
info_s *stegx_init(stegx_choices_s * choices);

/**
 * @brief Procédure de fin d'utilisation de la bibliothèque.
 * @req Avoir appelé \r{stegx_init} sur le paramètre "infos".
 * @sideeffect Libère la mémoire de la structure \r{info_s}.
 * @param infos Structure à libérer. 
 * @author Clément Caumes et Pierre Ayoub
 */
void stegx_clear(info_s * infos);

/**
 * @brief Vérifie la compatibilité des fichiers.
 * @sideeffect Remplit le champ \r{info_s.host.type} de la structure \r{info_s}.
 * @error \r{ERR_CHECK_COMPAT} si la vérification de la compatibilité a échouée.
 * @param infos Structure privée qui contient les informations pour réaliser la
 * dissimulation et l'extraction. 
 * @return 0 si la vérification s'est bien déroulée, sinon 1 en cas d'erreur et
 * met à jour \r{stegx_errno}.
 * @author Clément Caumes et Yassin Doudouh
 */
int stegx_check_compatibility(info_s * infos);

/** 
 * @brief Propose les algorithmes de stéganographie utilisables.
 * @details En fonction des informations rentrées dans \r{info_s}, vérifie pour
 * chaque algorithme qu'il est utilisable en fonction type de fichier hôte et de
 * la taille du fichier à cacher.
 * @sideeffect Initialise et rempli le champ \r{info_s.hidden_length} et 
 * \r{info_s.host.file_info} ainsi que la variable globale \r{stegx_propos_algos}.
 * @error \r{ERR_SUGG_ALGOS} s'il y a eu un problème d'utilisation.
 * @error \r{ERR_LENGTH_HIDDEN} si le fichier à cacher est supérieur à la taille
 * maximum (2^32).
 * @param infos Structure contenant les informations concernant la dissimulation.
 * @return 0 si tout se passe bien, sinon 1 en cas d'erreur et met à jour
 * \r{stegx_errno}. 
 * @author Clément Caumes et Yassin Doudouh
 */
int stegx_suggest_algo(info_s * infos);

/** 
 * @brief Initialise l'algorithme à utiliser pour la dissimulation. 
 * @details A partir de la variable globale \r{stegx_propos_algos} et de
 * "algo_choosen", cette fonction permet de choisir l'algorithme à utiliser.
 * @sideeffect Remplit le champ \r{info_s.algo} avec l'algorithme choisi, ainsi
 * que le champ \r{info_s.passwd} avec un mot de passe aléatoire si
 * l'utilisateur n'en à pas choisi.
 * @error \r{ERR_SUGG_ALGOS} si la fonction est mal utilisée (mauvais mode).
 * @error \r{ERR_CHOICE_ALGO} si l'algorithme choisi n'est pas disponible.
 * @param infos Structure contenant les informations concernant la dissimulation.
 * @param algo_choosen Algorithme choisi par l'utilisateur. 
 * @return 0 si la fonction s'est déroulée normalement, 1 en cas d'erreur et met
 * à jour la variable \r{stegx_errno}.
 * @author Clément Caumes et Yassin Doudouh
 */
int stegx_choose_algo(info_s * infos, algo_e algo_choosen);

/** 
 * @brief Detecte l'algorithme utilisé ainsi que le nom du fichier caché et la
 * taille des données cachées.
 * @sideeffect Remplit les champs \r{info_s.host.file_info}, \r{info_s.algo}, 
 * \r{info_s.method}, \r{info_s.hidden_length}, \r{info_s.hidden_name}.
 * @param infos Structure représentant les informations concernant la dissimulation.
 * @error \r{ERR_DETECT_ALGOS} si une erreur quelconque survient.
 * @error \r{ERR_NEED_PASSWD} si l'utilisateur n'as pas spécifié le mot de passe
 * requis.
 * @return 0 si la détection s'est bien déroulée, sinon 1 en cas d'erreur et met à jour
 * \r{stegx_errno}.
 * @author Pierre Ayoub et Damien Delaunay
 */
int stegx_detect_algo(info_s * infos);

/** 
 * @brief Va faire l'insertion selon l'algorithme, ainsi que les 
 * fichiers en entrée choisis par l'utilisateur. 
 * @error \r{ERR_INSERT} si une erreur survient durant la dissimulation.
 * @param infos Structure représentant les informations concernant la dissimulation.
 * @return 0 si l'insertion s'et bien passé, sinon 1 en cas d'erreur et met à
 * jour \r{stegx_errno}.
 * @author Clément Caumes et Pierre Ayoub
 */
int stegx_insert(info_s * infos);

/** 
 * @brief Va faire l'extraction selon l'algorithme détecté, ainsi que les 
 * fichiers en entrée choisis par l'utilisateur. 
 * @sideeffect Remplit le champ \r{res}.
 * @error \r{ERR_EXTRACT} si une erreur survient lors de l'extraction.
 * @param infos Structure représentant les informations concernant l'extraction.
 * @return 0 si l'extraction s'est bien passé, sinon 1 en cas d'erreur et met à jour
 * \r{stegx_errno}.
 * @author Clément Caumes et Pierre Ayoub
 */
int stegx_extract(info_s * infos, char *res_path);

#endif                          /* ifndef STEGX_H */
