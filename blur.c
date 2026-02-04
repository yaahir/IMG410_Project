#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

static void error_exit(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(1);
}

static void error_errno(const char *msg) {
    fprintf(stderr, "Error: %s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
  Read next integer from a P3 PPM file.
  - Skips whitespace
  - Skips comments that start with '#'
  Returns 1 if an int was read, 0 if EOF.
*/
static int read_int(FILE *fp, int *out) {
    int c;

    // Skip whitespace + comments
    while (1) {
        c = fgetc(fp);
        if (c == EOF) return 0;

        if (isspace(c)) continue;

        if (c == '#') {
            // skip until end of line
            while ((c = fgetc(fp)) != EOF && c != '\n') { }
            if (c == EOF) return 0;
            continue;
        }

        // Start of token
        ungetc(c, fp);
        break;
    }

    // fscanf will now read the int
    if (fscanf(fp, "%d", out) != 1) {
        error_exit("Bad integer in file");
    }
    return 1;
}

static int clamp(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Error: Usage: blur input.ppm output.ppm\n");
        return 2;
    }

    const char *in_path = argv[1];
    const char *out_path = argv[2];

    FILE *in = fopen(in_path, "rb");
    if (!in) error_errno("Could not open input file");

    // Read magic
    char magic[3] = {0};
    if (fscanf(in, "%2s", magic) != 1) error_exit("Could not read PPM header");
    if (strcmp(magic, "P3") != 0) error_exit("Only P3 PPM format is supported");

    int w, h, maxv;
    if (!read_int(in, &w) || !read_int(in, &h) || !read_int(in, &maxv))
        error_exit("Missing width/height/maxval");

    if (w <= 0 || h <= 0) error_exit("Width and height must be positive");
    if (maxv <= 0 || maxv > 65535) error_exit("Max color value must be 1..65535");

    long long nvals = (long long)w * (long long)h * 3LL;
    if (nvals <= 0 || nvals > 200000000LL) error_exit("Image too large");

    int *pix = (int*)malloc((size_t)nvals * sizeof(int));
    int *outpix = (int*)malloc((size_t)nvals * sizeof(int));
    if (!pix || !outpix) error_exit("Out of memory");

    // Read pixels
    for (long long i = 0; i < nvals; i++) {
        int v;
        if (!read_int(in, &v)) error_exit("Unexpected EOF in pixel data");
        if (v < 0 || v > maxv) error_exit("Pixel value out of range");
        pix[i] = v;
    }
    fclose(in);

    // 5x5 Gaussian kernel
    int K[5][5] = {
        {1, 2, 3, 2, 1},
        {2, 4, 6, 4, 2},
        {3, 6, 9, 6, 3},
        {2, 4, 6, 4, 2},
        {1, 2, 3, 2, 1}
    };
    const int KSUM = 81;

    // Blur
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            for (int ch = 0; ch < 3; ch++) {
                long long acc = 0;
                for (int ky = -2; ky <= 2; ky++) {
                    int sy = clamp(y + ky, 0, h - 1);
                    for (int kx = -2; kx <= 2; kx++) {
                        int sx = clamp(x + kx, 0, w - 1);
                        int kval = K[ky + 2][kx + 2];
                        long long idx = ((long long)sy * w + sx) * 3 + ch;
                        acc += (long long)kval * (long long)pix[idx];
                    }
                }

                // rounded divide by 81, then clamp
                int val = (int)((acc + (KSUM / 2)) / KSUM);
                val = clamp(val, 0, maxv);
                long long out_idx = ((long long)y * w + x) * 3 + ch;
                outpix[out_idx] = val;
            }
        }
    }

    // Write output
    FILE *out = fopen(out_path, "wb");
    if (!out) error_errno("Could not open output file");

    fprintf(out, "P3\n%d %d\n%d\n", w, h, maxv);

    // write values with line breaks to keep it readable
    int per_line = 15;
    int count = 0;
    for (long long i = 0; i < nvals; i++) {
        fprintf(out, "%d", outpix[i]);
        count++;
        if (count % per_line == 0) fprintf(out, "\n");
        else fprintf(out, " ");
    }
    if (count % per_line != 0) fprintf(out, "\n");

    fclose(out);
    free(pix);
    free(outpix);
    return 0;
}
