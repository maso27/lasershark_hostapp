/*
lasershark_stdin_edgeline.c - Application that draws a square along the outer perimeter of the galvanometer space
for consumption by lasershark_stdin.
Intended for use in determining how square the working envelope is.
Copyright (C) 2012 Jeffrey Nelson <nelsonjm@macpod.net>
2017 Mason Stone

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

#define MIN_VAL 0
#define MAX_VAL 4095

void print_help(const char* prog_name, FILE* stream)
{
    fprintf(stream, "\n %s [OPTIONS] - Draws a line along the periphery.\n", prog_name);
    fprintf(stream, "\nUseful for establishing a viable display envelope and compensating for distortion.\n\n");

    fprintf(stream, "Options:\n");
    fprintf(stream, "\t-h\tPrint this help text.\n");

    fprintf(stream, "Edge to display:\n (default is all four sides)\n");
    fprintf(stream, "\t-L\tDraw a line along the left edge.\n");
    fprintf(stream, "\t-R\tDraw a line along the right edge.\n");
    fprintf(stream, "\t-T\tDraw a line across the top edge.\n");
    fprintf(stream, "\t-B\tDraw a line across the bottom edge.\n");

    fprintf(stream, "Scaling:\n (default is 1)\n");
    fprintf(stream, "\t-X\tAmount to scale x-axis.\n");
    fprintf(stream, "\t-Y\tAmount to scale y-axis.\n");

    fprintf(stream, "Distortion Correction:\n");
    fprintf(stream, "\t-E\tE-Factor correction (default is 1).\n");
    fprintf(stream, "\t-M\tM-Factor correction (default is 0).\n");

    fprintf(stream, "Display Speed:\n");
    fprintf(stream, "\t-s\tSize of the step between each value (default is 8).\n");
    fprintf(stream, "\t-r\tRate to display samples at. Must be between 1 and 30000 (default is 30000).\n");
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


int main (int argc, char *argv[])
{
    int count; // varying value of x and y depending on location within the square
    int step = 8; // number of "pixels" to skip
    int c;

    double x_scalar = 1.0;
    double y_scalar = 1.0;
    double m_factor = 0.0;
    double e_factor = 1.0;

    int rate = 30000;
    int Aflag = 1;
    int Lflag = 0;
    int Rflag = 0;
    int Tflag = 0;
    int Bflag = 0;
    int hflag = 0;
    int sflag = 0;
    int Xflag = 0;
    int Yflag = 0;
    int Mflag = 0;
    int Eflag = 0;
    int rflag = 0;
    
    opterr_portable = 1;
    while (-1 != (c =getopt_portable(argc, argv, "LRTBhs:X:Y:M:E:r:"))) {
        switch(c) {
        case 'L':
            Lflag++;
            Aflag = 0;
            break;
        case 'R':
            Rflag++;
            Aflag = 0;
            break;
        case 'T':
            Tflag++;
            Aflag = 0;
            break;
        case 'B':
            Bflag++;
            Aflag = 0;
            break;
        case 'h':
            hflag++;
            break;
        case 's':
            sflag++;
            step = atoi(optarg_portable);
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
        case 'r':
            rflag++;
            rate = atoi(optarg_portable);
            break;
        default:
            break;
        }
    }

    // error handling
    if (Lflag > 1 || Rflag > 1 || Tflag > 1 || Bflag > 1 ||
            hflag > 1 || sflag > 1 || Xflag > 1 || Yflag > 1 || 
            Mflag > 1 || Eflag > 1 || rflag > 1) {
        fprintf(stderr, "Cannot specify flags more than once.\n");
        print_help(argv[0], stderr);
        exit(1);
    }

    if (Lflag + Rflag + Tflag + Bflag > 1) {
        fprintf(stderr, "Cannot specify more than one edge.\n");
        print_help(argv[0], stderr);
        exit(1);
    }

    if (step < MIN_VAL || step > MAX_VAL) {
        fprintf(stderr, "Step size must be between %d and %d.\n", MIN_VAL, MAX_VAL);
        print_help(argv[0], stderr);
        exit(1);
    }

    if (rate < 0 || rate > 30000) {
        fprintf(stderr, "Rate must be between 1 and 30,000.\n");
        print_help(argv[0], stderr);
        exit(1);
    }

    if (Mflag || Eflag) {
        if (m_factor * pow(abs(MAX_VAL/2),e_factor) > 1) {
            fprintf(stderr, "M and E factors will result in failure. Try Different Values.\n");
            exit(1);
        }
    }

    if (hflag) {
        print_help(argv[0], stdout);
        exit(1);
    }


    printf("r=%d\n",rate);
    printf("e=1\n");

    while(1) {

        // top sweep
        if (Aflag == 1 || Tflag == 1)
            for (count = MIN_VAL; count < MAX_VAL; count += step)
                send_scaled(x_scalar, y_scalar, m_factor, e_factor,
                    count, MIN_VAL, 4095, 4095, 1, 1); // x_scalar, y_scalar, m_factor, e_factor, x, y, a, b, c, intl_a

        // top sweep back
        if (Tflag == 1)
            for (count = MAX_VAL; count > MIN_VAL; count -= step)
                send_scaled(x_scalar, y_scalar, m_factor, e_factor,
                    count, MIN_VAL, 4095, 4095, 1, 1); // x_scalar, y_scalar, m_factor, e_factor, x, y, a, b, c, intl_a

        // down right side
        if (Aflag == 1 || Rflag == 1)
            for (count = MIN_VAL; count < MAX_VAL; count += step)
                send_scaled(x_scalar, y_scalar, m_factor, e_factor,
                    MAX_VAL, count, 4095, 4095, 1, 1); // x_scalar, y_scalar, m_factor, e_factor, x, y, a, b, c, intl_a

        // back up right side
        if (Rflag == 1)
            for (count = MAX_VAL; count > MIN_VAL; count -= step)
                send_scaled(x_scalar, y_scalar, m_factor, e_factor,
                    MAX_VAL, count, 4095, 4095, 1, 1); // x_scalar, y_scalar, m_factor, e_factor, x, y, a, b, c, intl_a

        // bottom sweep
        if (Aflag == 1 || Bflag == 1)
            for (count = MAX_VAL; count > MIN_VAL; count -= step)
                send_scaled(x_scalar, y_scalar, m_factor, e_factor,
                    count, MAX_VAL, 4095, 4095, 1, 1); // x_scalar, y_scalar, m_factor, e_factor, x, y, a, b, c, intl_a

        // bottom sweep back
        if (Bflag == 1)
            for (count = MIN_VAL; count < MAX_VAL; count += step)
                send_scaled(x_scalar, y_scalar, m_factor, e_factor,
                    count, MAX_VAL, 4095, 4095, 1, 1); // x_scalar, y_scalar, m_factor, e_factor, x, y, a, b, c, intl_a

        // up left side
        if (Aflag == 1 || Lflag == 1)
            for (count = MAX_VAL; count > MIN_VAL; count -= step)
                send_scaled(x_scalar, y_scalar, m_factor, e_factor,
                    MIN_VAL, count, 4095, 4095, 1, 1); // x_scalar, y_scalar, m_factor, e_factor, x, y, a, b, c, intl_a

        // back down left side
        if (Lflag == 1)
            for (count = MIN_VAL; count < MAX_VAL; count += step)
                send_scaled(x_scalar, y_scalar, m_factor, e_factor,
                    MIN_VAL, count, 4095, 4095, 1, 1); // x_scalar, y_scalar, m_factor, e_factor, x, y, a, b, c, intl_a

    }


    printf("f=1\n");
    printf("e=0\n");
    return 0;
}

