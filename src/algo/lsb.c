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
 * @file lsb.c
 * @brief Algorithme LSB.
 * @details Contient les fonctions concernant l'algorithme LSB.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "common.h"
#include "stegx_common.h"
#include "stegx_errors.h"
#include "protection.h"
#include "insert.h"
#include "rand.h"

/** MP3 : masque à appliquer au header où cacher un bit. */
static const uint32_t mp3_mask[MP3_HDR_NB_BITS_MODIF] = {0xFFFFFFFB, 0xFFFFFFF7, 0xFFFFFEFF};
/** MP3 : offset à appliquer au bit à caché / déjà caché en fonction du masque. */
static const uint32_t mp3_shift[MP3_HDR_NB_BITS_MODIF] = {2, 3, 8};

int protect_data_lsb(uint8_t * pixels, uint32_t pixels_length, uint8_t * data, uint32_t data_length,
                     char *passwd, mode_e mode)
{

    // Tableau pour savoir si cette case a deja ete inseree dans result
    uint8_t *done = malloc(pixels_length * sizeof(uint8_t));
    if (!done)
        return perror("Can't allocate memory protection data"), 1;
    uint32_t i;
    for (i = 0; i < pixels_length; i++) {
        done[i] = NOT_DONE;     // aucune case n'a ete encore inséré
    }

    // Si on est en extraction on met les donnees extraites a 0
    if (mode == STEGX_MODE_EXTRACT) {
        for (i = 0; i < data_length; i++) {
            data[i] = 0;        // aucune case n'a ete encore inséré
        }
    }
    // Création de la seed pour la generation pseudo aleatoires de nombres
    stegx_srand(create_seed(passwd));

    /* 
     * hidden_length_recalcul : Nombre d'éléments du tableau data au fur 
     * et a mesure que les elements ont ete inseres dans result.
     * l : Entier representant le nombre d'éléments passés (vus) dans le tableau
     * m : Entier representant le curseur sur done (pour savoir quel element a deja ete modifié)
     * */
    uint32_t hidden_length_recalcul = pixels_length + 1;
    uint32_t l, m, rang;
    uint8_t mask_hidden;
    uint8_t mask_host = 0xFC;   // 11111100 en binaire
    uint8_t mask_res = 0x03;    //00000011 en binaire
    uint8_t cpy_byte;

    int j;
    // pour chaque element a cacher
    for (i = 0; i < data_length; i++) {
        mask_hidden = 0xC0;     // 11000000 en binaire
        // pour chaque couple de bits dans l'octet (soit 4)
        for (j = 0; j < 4; j++) {
            l = m = 0;
            // on recherche un octet (composante de couleur où cacher les 2 bits) au hasard
            assert(hidden_length_recalcul != 1);
            rang = stegx_rand() % (hidden_length_recalcul - 1);       //-1 (au dernier tour rang vaudra 1)
            // Cas spécial : si rang == 0 on cherche le prochain element non modifié
            if (rang == 0) {
                while (done[m] == 1) {
                    m++;
                }
            }
            // Recherche du prochain element a modifier
            while (l < rang) {
                /* si on passe sur un element deja fait on incremente le curseur 
                 * mais pas le nombre d'éléments passés
                 */

                if (done[m] == DONE) {
                    m++;
                }
                // sinon on incremente le curseur et le nombre d'élements passés
                else {
                    l++;
                    m++;
                }
            }

            /* Cas spécial : si pendant la recherche, il ne manque plus qu'un 
             * élement non vu pour trouver l'élément a chercher , il suffit de 
             * chercher le prochain element non vu.
             * */
            if ((rang == l) && (done[m] == DONE)) {
                while (done[m] == DONE) {
                    m++;
                }
            }

            /* A cet endroit de l'algorithme, on connait l'élément a modifier/ a extraire
             * qui est a la case m */
            done[m] = DONE;
            hidden_length_recalcul--;

            // on remplace les 2 bits de poids faible par 2 bits de l'octet a cacher
            if (mode == STEGX_MODE_INSERT) {
                pixels[m] = (pixels[m] & mask_host) + ((data[i] & mask_hidden) >> (-2 * j + 6));
                /* -2*i+6 correspond a l'équation correspondant au déclage effectue en fonction 
                 * de la localisation des bits de l'octet a cacher
                 *
                 * i==0 -> decalage de 6 vers la gauche
                 * i==1 -> decalage de 4 vers la gauche
                 * i==2 -> decalage de 2 vers la gauche
                 * i==3 -> decalage de 0 vers la gauche
                 */
            } else if (mode == STEGX_MODE_EXTRACT) {
                // on veut obtenir les 2 bits de poids faibles
                cpy_byte = pixels[m] & mask_res;
                cpy_byte <<= (-2 * j + 6);      // equation pour trouver le decalage a faire
                data[i] = data[i] + cpy_byte;
            } else
                return 1;
            mask_hidden >>= 2;
        }
    }

    free(done);
    return 0;
}

int insert_lsb(info_s * infos)
{
    assert(infos);
    assert(infos->mode == STEGX_MODE_INSERT);
    assert(infos->algo == STEGX_ALGO_LSB);
    if (fseek(infos->host.host, 0, SEEK_SET) == -1)
        return perror("Can't make jump host file"), 1;
    if (fseek(infos->hidden, 0, SEEK_SET) == -1)
        return perror("Can't make jump hidden file"), 1;

    // pour le format BMP et WAVE
    assert(infos->host.type == BMP_UNCOMPRESSED || infos->host.type == WAV_PCM || infos->host.type == MP3);
    if (infos->host.type == BMP_UNCOMPRESSED || infos->host.type == WAV_PCM) {
        uint32_t nb_cpy = 0;        //nb doctets recopies
        uint8_t byte_read_hidden, byte_read_host;
        uint8_t mask_host, mask_hidden;
        int i;

        // Recopie du header dans le fichier resultat -> taille du header de l'hote
        while (nb_cpy < (infos->host.file_info.bmp.header_size)) {
            if (fread(&byte_read_host, sizeof(uint8_t), 1, infos->host.host) == 0)
                return perror("Can't read header host"), 1;
            if (fwrite(&byte_read_host, sizeof(uint8_t), 1, infos->res) != 1)
                return perror("Sig: Can't write header host"), 1;
            nb_cpy++;
        }

        nb_cpy = 0;
        if (fseek(infos->hidden, 0, SEEK_SET) == -1)
            return perror("Can't make jump hidden file"), 1;

        /* Si la taille du fichier a cacher ou le nombre de pixels est 
         * trop importante -> LSB sur les pixels dans l'ordre d'écriture 
         * dans le fichier hote */
        if ((infos->hidden_length > LENGTH_FILE_MAX || infos->host.type == WAV_PCM)
            || (infos->host.file_info.bmp.data_size > LENGTH_FILE_MAX)) {
            mask_host = 0xFC;   // 11111100 en binaire

            stegx_srand(create_seed(infos->passwd));
            // Cacher en LSB les donnees du fichier a cacher
            while (nb_cpy < (infos->hidden_length)) {
                // Lecture de l'octet du fichier a cacher
                if (fread(&byte_read_hidden, sizeof(uint8_t), 1, infos->hidden) == 0)
                    return perror("Can't read data hidden"), 2;
                byte_read_hidden ^= stegx_rand() % UINT8_MAX;

                // pour chaque paire de bits dans un octet (soit 4)
                mask_hidden = 0xC0;     // 11000000 en binaire
                for (i = 0; i < 4; i++) {
                    if (fread(&byte_read_host, sizeof(uint8_t), 1, infos->host.host) == 0)
                        return perror("Can't read data host"), 1;
                    // on remplace les 2 bits de poids faible par 2 bits de l'octet a cacher
                    byte_read_host =
                        (byte_read_host & mask_host) +
                        ((byte_read_hidden & mask_hidden) >> (-2 * i + 6));
                    // -2*i+6 correspond a l'équation correspondant au déclage effectue en fonction 
                    // de la localisation des bits de l'octet a cacher
                    /*
                     * i==0 -> decalage de 6 vers la gauche
                     * i==1 -> decalage de 4 vers la gauche
                     * i==2 -> decalage de 2 vers la gauche
                     * i==3 -> decalage de 0 vers la gauche
                     */
                    if (fwrite(&byte_read_host, sizeof(uint8_t), 1, infos->res) != 1)
                        return perror("Sig: Can't write data host modified"), 1;
                    mask_hidden >>= 2;
                }
                nb_cpy++;
            }

            // represente le nombre d'octets a copier de host vers res
            uint32_t rest_host_cpy = (infos->host.file_info.bmp.data_size) - nb_cpy * 4;
            nb_cpy = 0;
            // Recopie du reste des donnees de l'hote -> taille de data de l'hote - taille des octets utilisés pour cacher
            while (rest_host_cpy != 0) {
                if (fread(&byte_read_host, sizeof(uint8_t), 1, infos->host.host) == 0)
                    return perror("Can't read data host"), 1;
                if (fwrite(&byte_read_host, sizeof(uint8_t), 1, infos->res) == 0)
                    return perror("Can't write data host"), 1;
                rest_host_cpy--;
            }
        }

        /* Sinon on utilise la methode de protection des donnees pour 
         * cacher les octets dans des pixels aleatoires */
        else {
            // Lecture des pixels dans lesquels on va cacher le fichier a cacher
            uint32_t data_length;
            uint8_t *pixels = malloc((infos->host.file_info.bmp.data_size) * sizeof(uint8_t));
            if (!pixels)
                return perror("Can't allocate memory Insertion"), 1;
            for (data_length = 0; data_length < infos->host.file_info.bmp.data_size; data_length++) {
                if (fread(&byte_read_host, sizeof(uint8_t), 1, infos->host.host) == 0)
                    return perror("Can't read header host"), 1;
                pixels[data_length] = byte_read_host;
            }

            // Lecture des donnees a cacher qui seront stockées dans data
            uint8_t *data = malloc((infos->hidden_length) * sizeof(uint8_t));
            if (!data)
                return perror("Can't allocate memory Insertion"), 1;
            for (data_length = 0; data_length < infos->hidden_length; data_length++) {
                if (fread(&byte_read_host, sizeof(uint8_t), 1, infos->hidden) == 0)
                    return perror("Can't read header hidden"), 1;
                data[data_length] = byte_read_host;
            }
            /* methode de protection des donnees avec insertion sur les bits de 
             * poids faible de pixels aleatoires */
            protect_data_lsb(pixels, infos->host.file_info.bmp.data_size, data,
                             infos->hidden_length, infos->passwd, infos->mode);

            for (data_length = 0; data_length < infos->host.file_info.bmp.data_size; data_length++) {
                if (fwrite(&pixels[data_length], sizeof(uint8_t), 1, infos->res) != 1)
                    return perror("Sig: Can't write data host modified"), 1;
            }

            free(data);
            free(pixels);
        }

        // Ecriture de la signature
        if (write_signature(infos) == 1) {
            stegx_errno = ERR_INSERT;
            return 1;
        }
        return 0;
    }

    /* Insertion en LSB sur le format MP3. */
    if (infos->host.type == MP3) {
        /* Initialisation. */
        FILE * h = infos->host.host, * r = infos->res; // Fichier hôte et fichier résultant.
        mp3_s * hs = &(infos->host.file_info.mp3);     // Structure du fichier hôte.
        srand(create_seed(infos->passwd));

        /* Recopie du header ID3v2 du fichier hôte s'il y en à un. */
        uint8_t buf[BUFSIZ], b = 0; // Buffer, octet temporaire lu.
        for (int s = hs->fr_frst_adr; s && fread(buf, sizeof(*buf), s < BUFSIZ ? s : BUFSIZ, h);)
            s -= fwrite(buf, sizeof(*buf), s < BUFSIZ ? s : BUFSIZ, r);
        if (ferror(h) || ferror(r))
            return perror("insert_lsb MP3: Can't copy the header of the MP3 file"), 1;

        assert(ftell(h) == hs->fr_frst_adr);
        /* Lecture successive des headers de chaque frame. "b_cnt" et "hdr_cnt" sont
         * respectivement les compteurs des bits à traiter et traités de l'octet
         * et du header venant d'être lu.*/
        uint32_t hdr = 0; // Header lu.
        for (uint32_t hdr_cnt = 0, b_cnt = 0; fread(&hdr, sizeof(hdr), 1, h) && mp3_mpeg_hdr_test(hdr = stegx_be32toh(hdr)); hdr_cnt = 0) {
            /* S'il ne reste plus de données déjà lues à cacher, on relis. Si on peux relire, on remet le compteur "b_cnt" égal à 8 bits à cacher.
             * Tant qu'il reste des bits à caché dans l'octet lu et qu'on à pas saturé le header du MP3, on cache. On relis si besoin le fichier
             * à cacher pour saturer le header du MP3 jusqu'à ce qu'on ai tout lu. */
            for (; (b_cnt = !b_cnt ? fread(&b, sizeof(b), 1, infos->hidden) * 8 : b_cnt) && hdr_cnt < MP3_HDR_NB_BITS_MODIF;
                    b >>= 1, b_cnt--, hdr_cnt++) {
                /* Si on vient de lire un octet du fichier à cacher. */
                if (b_cnt == 8)
                    b ^= rand() % UINT8_MAX;
                hdr = (hdr & mp3_mask[hdr_cnt]) | ((b & 1) << mp3_shift[hdr_cnt]);
            }
            /* On écrit le header éventuellement modifié puis les données de la frame. */
            if (!fwrite((hdr = stegx_htobe32(hdr), &hdr), sizeof(hdr), 1, r) || mp3_mpeg_fr_write(stegx_be32toh(hdr), h, r))
                return perror("insert_lsb MP3: Can't write current MPEG header and frame"), 1;
        }
        if (ferror(h))
            return perror("insert_lsb MP3: Can't read frame header"), 1;

        /* Curseur sur un tag ID3v1 => écris le tag. */
        if (mp3_id3v1_hdr_test(hdr)) {
            if (!fwrite((hdr = stegx_htobe32(hdr), &hdr), sizeof(hdr), 1, r) || mp3_id3v1_tag_write(h, r))
                return perror("insert_lsb MP3: Can't write ID3v1 tag at the end of file"), 1;
        } 

        assert(ftell(h) == hs->eof);
        assert(feof(infos->hidden));
        /* Écriture de la signature et fin du LSB. */
        if (write_signature(infos))
            return stegx_errno = ERR_INSERT, 1;
        return 0;
    }

    // si les formats ne sont pas corrects erreur 
    return 1;
}

int extract_lsb(info_s * infos)
{
    assert(infos);
    assert(infos->mode == STEGX_MODE_EXTRACT);
    assert(infos->algo == STEGX_ALGO_LSB);
    if (fseek(infos->host.host, 0, SEEK_SET) == -1)
        return perror("extract_lsb: Can't jump to the beginning of the host file"), 1;

    // pour les formats BMP et WAVE
    if (infos->host.type == BMP_UNCOMPRESSED || infos->host.type == WAV_PCM) {
        uint32_t header_size;
        uint32_t nb_cpy;
        uint8_t byte_read_host;
        header_size = infos->host.file_info.bmp.header_size;

        // déplacement jusqu'au debut de l'image brute
        if (fseek(infos->host.host, header_size, SEEK_SET) == -1)
            return perror("Can't make extraction EOF"), 1;

        if ((infos->hidden_length > LENGTH_FILE_MAX || infos->host.type == WAV_PCM)
            || (infos->host.file_info.bmp.data_size > LENGTH_FILE_MAX)) {
            nb_cpy = 0;
            int i;
            uint8_t mask_host, byte_created;
            stegx_srand(create_seed(infos->passwd));

            mask_host = 0x03;   // 00000011 en binaire
            // Extraire en LSB les donnees du fichier a cacher -> taille du fichier a cacher
            while (nb_cpy < (infos->hidden_length)) {

                byte_created = 0;
                for (i = 0; i < 4; i++) {
                    // Lecture de l'octet du fichier hote
                    if (fread(&byte_read_host, sizeof(uint8_t), 1, infos->host.host) == 0)
                        return perror("Can't read data host"), 1;
                    byte_read_host = byte_read_host & mask_host;        // pour obtenir les 2 derniers bits
                    /*
                     * i==0 -> decalage de 6 vers la gauche
                     * i==1 -> decalage de 4 vers la gauche
                     * i==2 -> decalage de 2 vers la gauche
                     * i==3 -> decalage de 0 vers la gauche
                     */
                    byte_read_host <<= (-2 * i + 6);    // equation pour trouver le decalage a faire
                    byte_created = byte_created + byte_read_host;
                }
                byte_created ^= stegx_rand() % UINT8_MAX;
                if (fwrite(&byte_created, sizeof(uint8_t), 1, infos->res) != 1)
                    return perror("Sig: Can't write data hidden extracted"), 1;
                nb_cpy++;
            }
            return 0;
        }

        else {
            uint32_t data_length;
            uint8_t *pixels = malloc((infos->host.file_info.bmp.data_size) * sizeof(uint8_t));
            if (!pixels)
                return perror("Can't allocate memory Insertion"), 1;
            for (data_length = 0; data_length < infos->host.file_info.bmp.data_size; data_length++) {
                if (fread(&byte_read_host, sizeof(uint8_t), 1, infos->host.host) == 0)
                    return perror("Can't read header host"), 1;
                pixels[data_length] = byte_read_host;
            }
            uint8_t *data = malloc((infos->hidden_length) * sizeof(uint8_t));
            if (!data)
                return perror("Can't allocate memory Insertion"), 1;

            protect_data_lsb(pixels, infos->host.file_info.bmp.data_size, data,
                             infos->hidden_length, infos->passwd, infos->mode);

            for (data_length = 0; data_length < infos->hidden_length; data_length++) {
                if (fwrite(&data[data_length], sizeof(uint8_t), 1, infos->res) != 1)
                    return perror("Sig: Can't write data host modified"), 1;
            }

            free(pixels);
            free(data);
            return 0;
        }
    }

    /* Extraction en LSB sur le format MP3. */
    if (infos->host.type == MP3) {
        /* Initialisation. */
        FILE * h = infos->host.host, * r = infos->res; // Fichier hôte et fichier résultant.
        mp3_s * hs = &(infos->host.file_info.mp3);     // Structure du fichier hôte.
        srand(create_seed(infos->passwd));

        /* Saut du header ID3v2 du fichier hôte s'il y en à un. */
        if (fseek(h, hs->fr_frst_adr, SEEK_SET))
            return perror("extract_lsb: Can't jump over ID3v2 tag"), -1;

        assert(ftell(h) == hs->fr_frst_adr);
        /* Lecture successive des headers de chaque frame tant qu'on à pas fini
         * d'écrire la taille du fichier qui était caché dans le fichier
         * résultat. "s" correspond à la taille actuellement écrite dans le
         * fichier resultat. "hdr" est le header lu. "b_cnt" et "hdr_cnt" sont
         * respectivement les compteurs des bits à traiter et traités de l'octet
         * et du header venant d'être lu.*/
        uint8_t b = 0; // Octet reconstitué à écrire dans le résultat.
        for (uint32_t s = 0, hdr = 0, hdr_cnt = 0, b_cnt = 8; fread(&hdr, sizeof(hdr), 1, h) && s < infos->hidden_length; hdr_cnt = 0) {
            hdr = stegx_be32toh(hdr);
            /* Tant que l'on à pas lu les bits caché du header en cours. */
            for (; hdr_cnt < MP3_HDR_NB_BITS_MODIF; b_cnt--, hdr_cnt++) {
                /* Si notre octet est complètement reconstitué. */
                for (; !b_cnt ; b = 0, s++) {
                    b ^= rand() % UINT8_MAX;
                    b_cnt = fwrite(&b, sizeof(b), 1, r) * 8;
                }
                b |= ((hdr & ~mp3_mask[hdr_cnt]) >> mp3_shift[hdr_cnt]) << (8 - b_cnt);
            }
            /* On saute la frame quand on à récupéré tout les bits du header. */
            if (mp3_mpeg_fr_seek(hdr, h))
                return perror("insert_lsb MP3: Can't skip current MP3 MPEG frame"), 1;
        }
        if (ferror(h) || ferror(r))
            return perror("insert_lsb MP3: Can't read the host file or write the res file"), 1;
        return 0;
    }

    // si le format du fichier n'est pas correct -> renvoie une erreur
    return 1;
}
