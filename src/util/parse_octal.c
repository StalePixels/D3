//
// Created by D Rimron-Soutter on 06/11/2020.
//

#include <stdio.h>

#include "parse_octal.h"

uint32_t parse_octal(const char *original_value, size_t number_len) {
    uint32_t  octal_val = 0;

    while ((*original_value < '0' || *original_value > '7') && number_len > 0) {
        ++original_value;
        --number_len;
    }
    while (*original_value >= '0' && *original_value <= '7' && number_len > 0) {

        octal_val *= 8;
        octal_val += *original_value - '0';
        ++original_value;
        --number_len;
    }
    return (octal_val);
}
