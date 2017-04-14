/*
lasershark_stdin_printimage.c - Application that draws a given png
for consumption by lasershark_stdin.
Copyright (C) 2015 Jeffrey Nelson <nelsonjm@macpod.net>
2017 Mason Stone

This file is part of Lasershark's USB Host App.

Lasershark is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

Lasershark is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Lasershark. If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "getopt_portable.h"
#include "lodepng/lodepng.h"

#define MIN_VAL 0
#define MID_VAL 2048
#define MAX_VAL 4095

#define MAX_WIDTH 4096
#define MAX_HEIGHT 4096

#define SAMPLE_BUFFER 20

void print_help(const char* prog_name, FILE* stream)
{
    fprintf(stream, "%s [OPTIONS] - Displays a 3D Printable png via LaserShark\n", prog_name);

    fprintf(stream, "Options:\n");
    fprintf(stream, "\t-h");
    fprintf(stream, "\tPrint this help text\n");

    fprintf(stream, "Amplitudes of analog outputs\n");
    fprintf(stream, "\t-a\tA-channel minimum value\n");
    fprintf(stream, "\t-A\tA-channel maximum value\n");
    fprintf(stream, "\t-b\tB-channel minimum value\n");
    fprintf(stream, "\t-B\tB-channel maximum value\n");

    fprintf(stream, "Direction for raster sweep\n (Default is horizontal)\n");
    fprintf(stream, "\t-x\tHorizontal raster sweep\n");
    fprintf(stream, "\t-y\tVertical raster sweep\n");

    fprintf(stream, "Scaling:\n (default is 1)\n");
    fprintf(stream, "\t-X\tAmount to scale x-axis.\n");
    fprintf(stream, "\t-Y\tAmount to scale y-axis.\n");

    fprintf(stream, "Distortion Correction:\n");
    fprintf(stream, "\t-E\tE-Factor correction (default is 1).\n");
    fprintf(stream, "\t-M\tM-Factor correction (default is 0).\n");

    fprintf(stream, "File input\n");
    fprintf(stream, "\t-p\tPNG to print. Must be less than or equal to 4096x4096 in size\n");

    fprintf(stream, "Sweep speed\n");
    fprintf(stream, "\t-r\tRate to display samples at. Must be between 1 and 30,000\n");
}

// a quick routine to round to the nearest integer
int roundNum(double num)
{
    return num < 0 ? num - 0.5 : num + 0.5;
}

// apply scalar factors to values and print them out
void send_scaled(double x_scalar, double y_scalar, double m_factor, double e_factor, int x, int y, int a, int b, int c, int intl_a)
{
    int shift_amt = roundNum(MIN_VAL + MAX_VAL) / 2; // midway between MIN_VAL and MAX_VAL
    int x_shifted, y_shifted;
    int new_x, new_y;

    x_shifted = x - shift_amt;
    y_shifted = y - shift_amt;

    if ((m_factor != 0.0) || (e_factor != 1.0)) { // if distortion correction has been requested
        x_scalar *= 1 - (m_factor * pow(abs(y_shifted),e_factor));  // x = M * y^e parabola , removed from the scalar
//        y_scalar *= 1 - (m_factor * pow(abs(x_shifted),e_factor));  //  (Note that M should be tiny)
    }

    new_x = roundNum(x_shifted * x_scalar) + shift_amt; // have to center it before multiplying, then shift back
    new_y = roundNum(y_shifted * y_scalar) + shift_amt;

    if ((new_x >= MIN_VAL) && (new_x <= MAX_VAL) && (new_y >= MIN_VAL) && (new_y <= MAX_VAL))
        printf("s=%u,%u,%u,%u,%u,%u\n",
            new_x, new_y, a, b, c, intl_a); // x, y, a, b, c, intl_a

    return;
}

// return a 1 if any value in an array isn't zero
int orArray(int array_size, int *array)
{
    int i;
    for (i = 0; i < array_size; i++)
        if (array[i] != 0) return 1;
    return 0;
}


int main (int argc, char *argv[])
{
    int rc;
    int ret = 1;
    int c;

    unsigned int w, h;
    unsigned int w_off, h_off;
    uint16_t *image;

    double x_scalar = 1.0;
    double y_scalar = 1.0;
    double m_factor = 0.0;
    double e_factor = 1.0;

    int sample_count;
    int count = 0;
    int tmp_pos;
    unsigned int curr_x_pos = 0, curr_y_pos = 0;
    unsigned int future_x_pos = 0, future_y_pos = 0; // these will represent an advanced location for look-ahead
    unsigned int a_min = MIN_VAL;
    unsigned int a_max = MAX_VAL;
    unsigned int b_min = MIN_VAL;
    unsigned int b_max = MAX_VAL;
    unsigned int a_val;
    unsigned int b_val;
    int c_val_future; // looking ahead to so we can enable blanking before the image starts
    int c_val;
    int c_val_buffer[SAMPLE_BUFFER]; // tracking the last <SAMPLE_BUFFER> number of pixels
    int buff_offset = SAMPLE_BUFFER/2; // halfway through the buffer (note that it's truncated deliberately
    int buff_ctr_future = buff_offset; // rolling count to track future location within c_val_buffer
    int buff_ctr = 0; // current pixel value

    int aflag = 0;
    int Aflag = 0;
    int bflag = 0;
    int Bflag = 0;
    int hflag = 0;
    int horizflag = 0;
    int Xflag = 0;
    int Yflag = 0;
    int Mflag = 0;
    int Eflag = 0;
    int vertflag = 0;
    int pflag = 0;
    char* path = NULL;
    int rflag = 0;
    int rate = 20000;


    opterr_portable = 1;
    while (-1 != (c =getopt_portable(argc, argv, "a:A:b:B:hxyX:Y:M:E:p:r:"))) {    // setting flags
        switch(c) {
        case 'a':
            aflag++;
            a_min = atoi(optarg_portable);
            break;
        case 'A':
            Aflag++;
            a_max = atoi(optarg_portable);
            break;
        case 'b':
            bflag++;
            b_min = atoi(optarg_portable);
            break;
        case 'B':
            Bflag++;
            b_max = atoi(optarg_portable);
            break;
        case 'h':
            hflag++;
            break;
        case 'x':
            horizflag++;
            break;
        case 'y':
            vertflag++;
            break;
        case 'X':
            Xflag++;
            x_scalar = atof(optarg_portable);
            break;
        case 'Y':
            Yflag++;
            y_scalar = atof(optarg_portable);
            break;
        case 'M':
            Mflag++;
            m_factor = atof(optarg_portable);
            break;
        case 'E':
            Eflag++;
            e_factor = atof(optarg_portable);
            break;
        case 'p':
            pflag++;
            path = optarg_portable;
            break;
        case 'r':
            rflag++;
            rate = atoi(optarg_portable);
            break;
        default:
            print_help(argv[0], stderr);
            exit(1);
        }
    }

    // error handling
    if (aflag > 1 || Aflag > 1 || bflag > 1 || Bflag > 1 ||
            hflag > 1 || horizflag > 1 || vertflag > 1 || 
            Xflag > 1 || Yflag > 1 || Mflag > 1 || Eflag > 1 || 
            rflag > 1 || pflag > 1) {
        fprintf(stderr, "Cannot specify flags more than once.\n");
        print_help(argv[0], stderr);
        goto out_post;
    }

    if (!pflag) {
        fprintf(stderr, "Must specify image to print\n");
        print_help(argv[0], stderr);
        goto out_post;
    }

    if (a_min < MIN_VAL || a_min > MAX_VAL || a_max < MIN_VAL || a_max > MAX_VAL) {
        fprintf(stderr, "A-channel min and max must be between %d and %d\n", MIN_VAL, MAX_VAL);
        print_help(argv[0], stderr);
        goto out_post;
    }

    if (a_min > a_max) {
        fprintf(stderr, "A-channel min cannot be greater than the A-channel max\n");
        print_help(argv[0], stderr);
        goto out_post;
    }

    if (b_min < MIN_VAL || b_min > MAX_VAL || b_max < MIN_VAL || b_max > MAX_VAL) {
        fprintf(stderr, "B-channel min and max must be between %d and %d\n", MIN_VAL, MAX_VAL);
        print_help(argv[0], stderr);
        goto out_post;
    }

    if (b_min > b_max) {
        fprintf(stderr, "B-channel min cannot be greater than the B-channel max\n");
        print_help(argv[0], stderr);
        goto out_post;
    }

    if (Mflag || Eflag) {
        if (m_factor * pow(abs(MAX_VAL/2),e_factor) > 1) {
            fprintf(stderr, "M and E factors will result in failure. Try Different Values.\n");
            goto out_post;
        }
    }

    if (rate < 0 || rate > 30000) {
        fprintf(stderr, "Rate must be between 1 and 30,000\n");
        print_help(argv[0], stderr);
        goto out_post;
    }

    if (hflag) {
        print_help(argv[0], stdout);
        goto out_post;
    }

    rc = lodepng_decode_file((unsigned char**)&image, &w, &h, path, LCT_RGB, 16);
    if (rc) {
        fprintf(stderr, "Error opening image: %s\n", lodepng_error_text(rc));
        goto out_post;
    }

    if (w > MAX_WIDTH || h > MAX_HEIGHT) {
        fprintf(stderr, "Image cannot be larger than 4096 pixels in width or height\n");
        goto out_post;
    }

    int i;
    for (i = 0; i < SAMPLE_BUFFER; i++) c_val_buffer[i] = 0; // initialize c_val_buffer to zero

    w_off = (MAX_WIDTH - w) / 2;
    h_off = (MAX_HEIGHT - h) / 2;
    sample_count = w * h;

    printf("r=%d\n", rate);
    printf("e=1\n");
    printf("p=Image dimensions: %d x %d\n", w, h);

    if (vertflag) future_y_pos = buff_offset;
    else future_x_pos = buff_offset;

    while (count < sample_count) {

        tmp_pos = ((future_y_pos*w + future_x_pos)*3);
        c_val_future = (((image[tmp_pos] +
                   image[tmp_pos + 1] +
                   image[tmp_pos + 2])/3 >> 4) > MID_VAL) ? 1 : 0; // looking ahead to fill the buffer

        // rolling the buffers
        if (buff_ctr_future < SAMPLE_BUFFER - 1) buff_ctr_future++;
        else buff_ctr_future = 0;

        if (buff_ctr < SAMPLE_BUFFER - 1) buff_ctr++;
        else buff_ctr = 0;

        // storing the future value of c_val (it's SAMPLE_BUFFER/2 ahead of real time)
        c_val_buffer[buff_ctr_future] = c_val_future;

        c_val = c_val_buffer[buff_ctr];
        a_val = (c_val) ? a_max : a_min;
        b_val = (c_val) ? b_max : b_min;

        // display output if there's a pixel within SAMPLE_BUFFER/2 samples, either ahead or behind
        if (orArray(SAMPLE_BUFFER, c_val_buffer)) // if any pixel within <SAMPLE_BUFFER> samples was 1
            send_scaled(x_scalar, y_scalar, m_factor, e_factor,
//            printf("s=%u,%u,%u,%u,%u,%u\n",
               curr_x_pos + w_off,  curr_y_pos + h_off, a_val, b_val, c_val_buffer[buff_ctr],1); // x, y, a, b, c, intl_a

        if (vertflag) { // vertical raster has been specified (note: at this point the first value will be screwy)
            if (curr_x_pos & 1) { // Odd column
                if (curr_y_pos == 0) {
                    curr_x_pos++;
                } else {
                    curr_y_pos--;
                    if (future_y_pos > 0) future_y_pos = curr_y_pos - buff_offset;
                }
            } else { // Even column
                if (curr_y_pos == h-1) {
                    curr_x_pos++;
                } else {
                    curr_y_pos++;
                    if (future_y_pos < h-1) future_y_pos = curr_y_pos + buff_offset;
                }
            }
            future_x_pos = curr_x_pos;

        }

        else { // horizontal raster by default
            if (curr_y_pos & 1) { // Odd row
                if (curr_x_pos == 0) {
                    curr_y_pos++;
                } else {
                    curr_x_pos--;
                    if (future_x_pos > 0) future_x_pos = curr_x_pos - buff_offset;
                }
            } else { // Even row
                if (curr_x_pos == w-1) {
                    curr_y_pos++;
                } else {
                    curr_x_pos++;
                    if (future_x_pos < w-1) future_x_pos = curr_x_pos + buff_offset;
                }
            }
            future_y_pos = curr_y_pos;
        }

        count++;
    }


    free(image);

    printf("f=1\n");
    printf("e=0\n");

    ret = 0;
out_post:
    return ret;
}

