//
// Created by D Rimron-Soutter on 06/11/2020.
//

#include <z80.h>
#include <arch/zxn.h>
#include <intrinsic.h>
#include <arch/zxn/esxdos.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "untar.h"

#include "../util/parse_octal.h"
#include "../util/create_file.h"
#include "../util/create_dir.h"

static int is_end_of_archive(const char *p) {
    int n;
    for (n = 511; n >= 0; --n)
        if (p[n] != '\0')
            return (0);
    return (1);
}

static int verify_checksum(const char *p)
{
    uint32_t  n, u = 0;
    for (n = 0; n < 512; ++n) {
        if (n < 148 || n > 155)
            /* Standard tar checksum adds unsigned bytes. */
            u += ((unsigned char *)p)[n];
        else
            u += 0x20;

    }
    uint32_t cs = parse_octal(p + 148, 8);
    return (u == cs);
}

void untar(unsigned char tar_in, const char *path)
{
    /*
     * Original Written by Tim Kientzle, March 2009.
     *
     * source: https://github.com/libarchive/libarchive/blob/master/contrib/untar.c
     *
     */
    char buff[512];
    unsigned char file_out = 0;
    size_t bytes_read;
    uint32_t filesize;

    printf("Extracting from %s\n", path);

    for (;;) {
        z80_bpoke(23692, 255);
        bytes_read = esxdos_f_read(tar_in, buff, 512);
        if (bytes_read < 512) {
            printf("Short read on %s: expected 512, got %d\n",
                   path, (int)bytes_read);
            return;
        }
        if (is_end_of_archive(buff)) {
            printf("End of %s\n", path);
            return;
        }
        if (!verify_checksum(buff)) {
            printf("Checksum failure\n");
            return;
        }
        filesize = parse_octal(buff + 124, 12);
        switch (buff[156]) {
            case '1':
                printf(" Ignoring hardlink %s\n", buff);
                break;
            case '2':
                printf(" Ignoring symlink %s\n", buff);
                break;
            case '3':
                printf(" Ignoring character device %s\n", buff);
                break;
            case '4':
                printf(" Ignoring block device %s\n", buff);
                break;
            case '5':
                printf(" Extracting dir %s\n", buff);
                create_dir(buff);
                filesize = 0;
                break;
            case '6':
                printf(" Ignoring FIFO %s\n", buff);
                break;
            default:
                printf(" Extracting file %s (%lu bytes)\n", buff, filesize);
                file_out = create_file(buff);
                break;
        }
        while (filesize > 0) {
            bytes_read = esxdos_f_read(tar_in, buff, 512);
            if (bytes_read < 512) {
                printf("Short read on %s: Expected 512, got %d\n",
                       path, (int)bytes_read);
                return;
            }
            if (filesize < 512)
                bytes_read = filesize;
            if (file_out != 0) {
                if (esxdos_f_write(file_out, buff, bytes_read) != bytes_read) {
                    printf("Failed write\n");
                    esxdos_f_close(file_out);
                    tar_in = 0;
                }
            }
            filesize -= bytes_read;
        }
        if (file_out) {
            esxdos_f_close(file_out);
            file_out = 0;
        }
    }
}