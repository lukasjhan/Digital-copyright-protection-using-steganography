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
 * @file sugg_algo.c
 * @brief Proposition des algorithmes.
 * @details Module qui contient les fonctions qui permettent de proposer et de
 * vérifier l'algorithme de stéganographie à utiliser, ainsi que la fonction de
 * lecture/d'analyse du fichier hôte.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include "endian.h"
#include "common.h"
#include "stegx_common.h"
#include "stegx_errors.h"
#include "rand.h"

/** 
 * @brief Teste si l'on peut utiliser l'algorithme LSB pour la dissimulation. 
 * @param infos Structure représentant les informations concernant la dissimulation.
 * @return 0 Si l'algorithme est proposé, 1 sinon.
 * @author Clément Caumes (BMP) et Pierre Ayoub (WAV)
 */
static int can_use_lsb(info_s * infos)
{
    assert(infos);
    /* Si le fichier hôte est un fichier BMP non compressé. Si le nombre de bits
     * utilisés pour coder la couleur de chaque pixel est inférieur ou égal à 8
     * (soit 256 couleurs), le deuxième en-tête est suivi d’une table des
     * couleurs utilisées dans l’image => LSB non proposé */
    if (infos->host.type == BMP_UNCOMPRESSED && infos->host.file_info.bmp.pixel_length > 8) {
        // calcul du nombre d'octets représentant l'image brute
        uint64_t nb_bits_pic =
            ((infos->host.file_info.bmp.pixel_length) * (infos->host.file_info.bmp.pixel_number)) /
            8;
        // calcul du nombre de bits modifiables pour l'algorithme LSB
        nb_bits_pic /= 4;

        // si la taille du fichier a cacher est ok -> LSB possible
        if ((infos->hidden_length * 8) <= nb_bits_pic)
            return 0;
    }

    /* Si le fichier hôte est un fichier WAVE-PCM. */
    else if (infos->host.type == WAV_PCM) {
        /* Calcul du nombre de bits modifiables pour l'algorithme LSB :
           ([nb d'octets de data] / ([nb de bits par sample] / 8)) = [nb de bit de poids faible d'audio]
           [nb de bit de poids faible d'audio] * 2 = [nb de bits modifiable]
           (car on modifie les 2 bits de poids faible par sample). */
        uint64_t nb_bits_modif =
            ((infos->host.file_info.wav.data_size) / (infos->host.file_info.wav.chunk_size / 8)) *
            2;

        /* Si le nombre de bit du fichier à cacher est inférieur ou égal au
         * nombre de bit modifiables => LSB utilisable. */
        if ((infos->hidden_length * 8) <= nb_bits_modif)
            return 0;
    }
    /* Si le fichier hote est un fichier MP3. */
    else if (infos->host.type == MP3 &&
            (infos->host.file_info.mp3.fr_nb * MP3_HDR_NB_BITS_MODIF >= infos->hidden_length * 8))
        return 0;
    /* Sinon, on ne peux pas utiliser LSB. */
    return 1;
}

/** 
 * @brief Teste si l'on peut utiliser l'algorithme EOF pour la dissimulation. 
 * @param infos Structure représentant les informations concernant la dissimulation.
 * @return 0 si l'algorithme est proposé, 1 sinon.
 * @author Clément Caumes
 */
static int can_use_eof(info_s * infos)
{
    assert(infos);
    // Pour tous les formats proposés par StegX sauf AVI, on propose EOF.
    return infos->host.type == AVI_COMPRESSED || infos->host.type == AVI_UNCOMPRESSED ? 1 : !IS_FILE_TYPE(infos->host.type);
}

/** 
 * @brief Teste si l'on peut utiliser l'algorithme Metadata pour la dissimulation. 
 * @param infos Structure représentant les informations concernant la dissimulation.
 * @return 0 si l'algorithme est proposé, 1 sinon.
 * @author Clément Caumes
 */
static int can_use_metadata(info_s * infos)
{
    assert(infos);
    /* On propose Metadata pour BMP & PNG seulement. */
    /* L'algorithme METADATA sur le format BMP consiste à insérer entre le 
     * header et le debut de l'image les données cachées. L'offset de l'image 
     * brute est écrit sur 4 octets et il ne faut pas dépasser cette taille 
     */
    if ((infos->host.type == BMP_COMPRESSED) || (infos->host.type == BMP_UNCOMPRESSED)) {
        uint64_t length = infos->hidden_length + infos->host.file_info.bmp.header_size
            + infos->host.file_info.bmp.header_size;
        // Si cela depasse 4 octets on ne propose pas METADATA
        if (length > BMP_METADATA_MAX)
            return 1;
        else
            return 0;
    } else if (infos->host.type == PNG)
        return 0;
    else
        return 1;
}

/** 
 * @brief Teste si l'on peut utiliser l'algorithme EOC pour la dissimulation. 
 * @param infos Structure représentant les informations concernant la dissimulation.
 * @return 0 si l'algorithme est proposé, 1 sinon.
 * @author Clément Caumes
 */
static int can_use_eoc(info_s * infos)
{
    assert(infos);
    // Pour le format FLV, on propose EOC. Pour tous le reste, on ne propose pas EOC.
    return infos->host.type == FLV ? 0 : 1;
}

/** 
 * @brief Teste si l'on peut utiliser l'algorithme Junk Chunk pour la dissimulation. 
 * @param infos Structure représentant les informations concernant la dissimulation.
 * @return 0 si l'algorithme est proposé, 1 sinon.
 * @author Clément Caumes
 */
static int can_use_junk_chunk(info_s * infos)
{
    assert(infos);
    // Pour le format AVI, on propose Junk Chunk. Pour tous le reste, on ne propose pas Junk Chunk.
    return (infos->host.type == AVI_COMPRESSED) || (infos->host.type == AVI_UNCOMPRESSED) ? 0 : 1;
}

int fill_host_info(info_s * infos)
{
    /* Vérifications + on set positionne au début du fichier. */
    assert(infos && infos->host.host);
    if (fseek(infos->host.host, 0, SEEK_SET))
        return perror("Can't move to the beginning of the host file"), 1;

    // Remplit la structure BMP de infos.host.file_info.
    // http://www.mysti2d.net/polynesie2/ETT/C044/31/Steganographie/index.html?Formatbmp.html
    if ((infos->host.type == BMP_COMPRESSED) || (infos->host.type == BMP_UNCOMPRESSED)) {
        uint32_t begin_pic, length_pic, pixel_width, pixel_height;
        uint16_t pixel_length;

        // lecture de la taille totale du fichier
        if (fseek(infos->host.host, BMP_DEF_LENGTH, SEEK_SET) == -1)
            return 1;
        if (fread(&length_pic, sizeof(uint32_t), 1, infos->host.host) != 1)
            return 1;

        // lecture de la taille du header (qui correspond au debut de data)
        if (fseek(infos->host.host, BMP_DEF_PIC, SEEK_SET) == -1)
            return 1;
        if (fread(&begin_pic, sizeof(uint32_t), 1, infos->host.host) != 1)
            return 1;

        infos->host.file_info.bmp.header_size = begin_pic;
        infos->host.file_info.bmp.data_size = length_pic - begin_pic;

        // lecture du nombre de bits par pixel
        if (fseek(infos->host.host, BMP_DEF_PIX_LENGTH, SEEK_SET) == -1)
            return 1;
        if (fread(&pixel_length, sizeof(uint16_t), 1, infos->host.host) != 1)
            return 1;
        infos->host.file_info.bmp.pixel_length = pixel_length;

        // lecture de la largeur de l'image
        if (fseek(infos->host.host, BMP_DEF_NB_PIXEL, SEEK_SET) == -1)
            return 1;
        if (fread(&pixel_width, sizeof(uint32_t), 1, infos->host.host) != 1)
            return 1;

        // lecture de la hauteur de l'image
        if (fread(&pixel_height, sizeof(uint32_t), 1, infos->host.host) != 1)
            return 1;
        infos->host.file_info.bmp.pixel_number = pixel_width * pixel_height;
        return 0;
    }
    // remplit la structure PNG de infos.host.file_info
    // http://www.libpng.org/pub/png/spec/1.2/PNG-Chunks.html
    else if (infos->host.type == PNG) {
        // lecture de la taille du chunk IHDR (header)
        if (fseek(infos->host.host, PNG_DEF_IHDR, SEEK_SET) == -1)
            return 1;
        uint32_t ihdr_length;
        if (fread(&ihdr_length, sizeof(uint32_t), 1, infos->host.host) != 1)
            return 1;
        ihdr_length = stegx_be32toh(ihdr_length);
        infos->host.file_info.png.header_size = PNG_DEF_IHDR + ihdr_length;

        uint32_t chunk_size, chunk_id;

        // Jump sur le premier chunk et lecture de son ID et de sa taille
        if (fseek(infos->host.host, LENGTH_SIG_PNG, SEEK_SET))
            return perror("PNG file: Can not move in the file"), 1;
        if (fread(&chunk_size, sizeof(uint32_t), 1, infos->host.host) != 1)
            return perror("PNG file: Can't read length of chunk"), 1;
        chunk_size = stegx_be32toh(chunk_size);
        if (fread(&chunk_id, sizeof(uint32_t), 1, infos->host.host) != 1)
            return perror("PNG file: Can't read ID of chunk"), 1;
        // on cherche le chunk IEND pour connaitre la taille du fichier
        while (chunk_id != SIG_IEND) {
            if (fseek(infos->host.host, chunk_size + LENGTH_CRC, SEEK_CUR))
                return perror("PNG file: Can not move in the file"), 1;
            if (fread(&chunk_size, sizeof(uint32_t), 1, infos->host.host) != 1)
                return perror("PNG file: Can't read size of chunk"), 1;
            chunk_size = stegx_be32toh(chunk_size);
            if (fread(&chunk_id, sizeof(uint32_t), 1, infos->host.host) != 1)
                return perror("PNG file: Can't read ID of chunk"), 1;
        }
        uint32_t file_length = ftell(infos->host.host) + LENGTH_IEND;
        infos->host.file_info.png.data_size = file_length - infos->host.file_info.png.header_size;
        return 0;
    }

    /* Remplit la structure WAVE. */
    else if ((infos->host.type == WAV_PCM) || (infos->host.type == WAV_NO_PCM)) {
        /* Lecture de tout les subchunk depuis le premier subchunk du header
         * jusqu'au subchunk "data". */
        uint32_t chunk_id = 0, chunk_size = WAV_SUBCHK1_ADDR;   /* ID et taille du chunk lu. */
        while (chunk_id != WAV_DATA_SIGN) {
            /* On saute le chunk venant d'être lu (pour le premier : on va à
             * l'adresse du premier subchunk). */
            if (fseek(infos->host.host, chunk_size, SEEK_CUR))
                return perror("WAVE file: Can't skip the subchunk currently read"), 1;
            /* Lecture de l'ID du subchunk. */
            if (fread(&chunk_id, sizeof(chunk_id), 1, infos->host.host) != 1)
                return perror("WAVE file: Can't read ID of subchunk"), 1;
            /* Lecture de la taille du subchunk. */
            if (fread(&chunk_size, sizeof(chunk_size), 1, infos->host.host) != 1)
                return perror("WAVE file: Can't read the size of subchunk"), 1;

            /* Cas spécial : quand on lit le subchunk fmt, on en profite pour
             * lire le nombre de bits par sample. */
            if (chunk_id == WAV_FMT_SIGN) {
                chunk_size -= WAV_FMT_BPS_OFF + sizeof(uint16_t);
                if (fseek(infos->host.host, WAV_FMT_BPS_OFF, SEEK_CUR))
                    return perror("WAVE file: Can't jump to bit per sample in the fmt subchunk"), 1;
                if (fread
                    (&(infos->host.file_info.wav.chunk_size), sizeof(uint16_t), 1,
                     infos->host.host) != 1)
                    return perror("WAVE file: Can't read number of bit per sample"), 1;
            }
        }
        /* Récupération de la taille totale du header et de la taille de data. */
        infos->host.file_info.wav.header_size = ftell(infos->host.host);
        infos->host.file_info.wav.data_size = chunk_size;
        return 0;
    }
    // remplit la structure FLV de infos.host.file_info
    // https://www.adobe.com/content/dam/acom/en/devnet/flv/video_file_format_spec_v10.pdf
    else if (infos->host.type == FLV) {
        uint8_t check_tags;
        uint32_t header_size;
        uint8_t tag_type;
        uint32_t data_size;
        uint32_t prev_tag_size;

        infos->host.file_info.flv.nb_video_tag = 0;
        infos->host.file_info.flv.nb_metadata_tag = 0;
        infos->host.file_info.flv.file_size = 0;
        fseek(infos->host.host, 4, SEEK_SET);

        //lecture du header
        fread(&check_tags, sizeof(check_tags), 1, infos->host.host);
        fread(&header_size, sizeof(header_size), 1, infos->host.host);

        header_size = stegx_be32toh(header_size);
        infos->host.file_info.flv.file_size += header_size;
        fseek(infos->host.host, 4, SEEK_CUR);
        infos->host.file_info.flv.file_size += 4;

        //lecture des tags
        while (fread(&tag_type, sizeof(tag_type), 1, infos->host.host) == 1) {

            if (tag_type == METATAG) {
                infos->host.file_info.flv.nb_metadata_tag += 1;
            } else if (tag_type == VIDEO_TAG) {
                infos->host.file_info.flv.nb_video_tag += 1;
            } else if (!(tag_type == AUDIO_TAG || tag_type == SCRIPT_DATA_TAG)) {
                break;
            }
            fread(&data_size, sizeof(data_size), 1, infos->host.host);
            //lecture de la taille des data
            data_size = stegx_be32toh(data_size);
            //passage en 24 bits      
            data_size >>= 8;
            //deplacement jusqu'au prochain previous tag size (data size + 6 octets qui comportent d'autres informations non utiles) 
            fseek(infos->host.host, data_size + 6, SEEK_CUR);
            //lecture du previous tag size 
            fread(&prev_tag_size, sizeof(prev_tag_size), 1, infos->host.host);
            prev_tag_size = stegx_be32toh(prev_tag_size);
            infos->host.file_info.flv.file_size += prev_tag_size + 4;
        }
        if (infos->mode == STEGX_MODE_INSERT
            && fread(&check_tags, sizeof(uint8_t), 1, infos->host.host))
            return
                perror
                ("Fichier flv ayant des données en fin de fichier. Fichier incompatible pour l'insertion."),
                1;
        return 0;
    }

    /* Fichier MP3. */
    else if (infos->host.type == MP3) {
        uint32_t hdr = 0;
        long int * n = &(infos->host.file_info.mp3.fr_nb);
        long int * f = &(infos->host.file_info.mp3.fr_frst_adr);
        FILE * h = infos->host.host;
        /* Déplacement et stockage de l'adresse du header de la première frame du MP3 (pour le "LSB"). */
        if ((*f = mp3_mpeg_fr_find_first(h)) == -1 || fseek(h, *f, SEEK_SET))
            return perror("MP3 fill_host_info: Can't find first MPEG 1/2 Layer III frame"), 1;
        /* Dénombrement du nombre de frame (pour "can_use_lsb"). */
        for (*n = 0 ; (fread(&hdr, sizeof(hdr), 1, h) == 1) && mp3_mpeg_hdr_test(hdr = stegx_be32toh(hdr)); (*n)++) {
            if (mp3_mpeg_fr_seek(hdr, h))
                return perror("MP3 fill_host_info: Can't skip current MP3 MPEG frame"), 1;
        }
        if (ferror(h))
            return perror("MP3 fill_host_info: Can't read frame header"), 1;

        /* Curseur sur un tag ID3v1 => saut au-dessus du tag et fin du fichier définitive. */
        if (mp3_id3v1_hdr_test(hdr) && mp3_id3v1_tag_seek(h))
            return perror("MP3 fill_host_info: Can't skip over ID3v1 tag at the end of file"), 1;
        /* Stockage de la fin du fichier (pour EOF). */
        if ((infos->host.file_info.mp3.eof = ftell(h)) == -1)
            return perror("MP3 fill_host_info: Can't get end-of-file address"), 1;
        /* Curseur n'était pas sur un tag ID3v1 et la fin du fichier n'as pas
         * été lu => on à lu 4 octets de données en trop (exemple, la signature). */
        if (!mp3_id3v1_hdr_test(hdr) && !feof(h))
            infos->host.file_info.mp3.eof -= sizeof(hdr);
        return 0;
    }

    /* Structure AVI : structure vide. */
    else if (infos->host.type == AVI_COMPRESSED || infos->host.type == AVI_UNCOMPRESSED)
        return 0;
    /* Format non reconnu => erreur. */
    else
        return 1;
}

int stegx_suggest_algo(info_s * infos)
{
    /* Test si on est en mode insertion, si oui, remplit la structure
       infos->host.file_info. */
    if (infos->mode == STEGX_MODE_EXTRACT || fill_host_info(infos))
        return stegx_errno = ERR_SUGG_ALGOS, 1;

    // Lecture de la taille du fichier à cacher.
    if (fseek(infos->hidden, 0, SEEK_END))
        return stegx_errno = ERR_SUGG_ALGOS, perror("Can't move to the end of hidden file"), 1;
    uint64_t read_hidden_length = ftell(infos->hidden);
    if (read_hidden_length == 0)
        return stegx_errno = ERR_HIDDEN_FILE_EMPTY, 1;
    // Précaution overflow.
    if (read_hidden_length >= UINT32_MAX)
        return stegx_errno = ERR_LENGTH_HIDDEN, 1;
    else
        infos->hidden_length = (uint32_t) read_hidden_length;

    /* Remplissage du tableau stegx_propos_algos pour savoir 
       quels algos sont proposés par l'application en fonction des entrées de
       l'utilisateur. */
    /* Les fonctions de ce tableau doivent être déclarés dans l'ordre de
     * l'énumération. */
    int (*can_use_algo[STEGX_NB_ALGO]) (info_s *) = {
    can_use_lsb, can_use_eof, can_use_metadata, can_use_eoc, can_use_junk_chunk};
    for (algo_e i = 0; i < STEGX_NB_ALGO; i++)
        stegx_propos_algos[i] = !(*can_use_algo[i]) (infos);
    return 0;
}

int stegx_choose_algo(info_s * infos, algo_e algo_choosen)
{
    if (infos->mode == STEGX_MODE_EXTRACT)
        return stegx_errno = ERR_SUGG_ALGOS, 1;
    /* Si l'utilisateur n'a pas choisi de mot de passe, on en crée un par défaut aléatoirement. */
    if (infos->method == STEGX_WITHOUT_PASSWD) {
        stegx_srand(time(NULL));
        free(infos->passwd);
        if (!(infos->passwd = calloc((LENGTH_DEFAULT_PASSWD + 1), sizeof(char))))
            return perror("Can't allocate memory for password string"), 1;
        // Génération de symboles ASCII >= 32 et <= 126.
        for (int i = 0; i < LENGTH_DEFAULT_PASSWD; i++)
            infos->passwd[i] = 32 + (stegx_rand() % 95);
    }

    assert(algo_choosen >= STEGX_ALGO_LSB && algo_choosen < STEGX_NB_ALGO);
    /* Test que l'algorithme choisis à bien été proposé comme étant possible. Si
     * oui, alors il est sauvegardé. Sinon, on lève une erreur. */
    if (stegx_propos_algos[algo_choosen])
        infos->algo = algo_choosen;
    else
        return stegx_errno = ERR_CHOICE_ALGO, 1;
    return 0;
}
