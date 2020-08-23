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
 * @file flv.c
 * @brief Fichier FLV.
 * @details Contient les informations (fonctions, types, structures) relatives
 * aux fichiers au format FLV.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "common.h"
#include "stegx_common.h"
#include "stegx_errors.h"

/** Signature d'un fichier FLV. */
#define SIG_FLV 0x564C46

type_e stegx_test_file_flv(FILE * file)
{
    assert(file);
    if (fseek(file, 0, SEEK_SET) == -1)
        return perror("Can't move to FLV signature"), -1;
    // lecture de la signature FLV
    uint32_t sig_read;
    if (fread(&sig_read, sizeof(uint32_t), 1, file) != 1)
        return perror("Can't read FLV signature"), -1;
    // on enleve 8 premiers bits car on soccupe des 3 derniers octets
    sig_read <<= 8;
    sig_read >>= 8;
    if (sig_read != SIG_FLV) {
        return UNKNOWN;
    }
    return FLV;
}

int insert_metadata_flv(info_s * infos)
{
    (void)infos;                /* Unused. */
    return 1;
}

int extract_metadata_flv(info_s * infos)
{
    (void)infos;                /* Unused. */
    return 1;
}
