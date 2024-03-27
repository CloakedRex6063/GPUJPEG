/**
 * Example demonstrating decoding JPEG to custom CUDA buffer. Decoded image is
 * then copied back to RAM and written to a PNM file.
 */

#include <cuda_runtime.h>
#include <libgpujpeg/gpujpeg_common.h>
#include <libgpujpeg/gpujpeg_decoder.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define strcasecmp _stricmp
#endif

#define MAX_OUT_LEN (7680 * 4320 * 4)

static void usage(const char *progname) {
        printf("Usage:\n");
        printf("\t%s <file>.jpg\n", progname);
}

static bool check_params(int argc, char *argv[]) {
        if (argc != 2 || strrchr(argv[1], '.') == NULL) {
                return false;
        }
        const char *ext = strrchr(argv[1], '.') + 1;
        return strcasecmp(ext, "jpg") == 0;
}

// to simplify deletion of allocated items on all paths
struct decode_data {
        char *out_filename;
        struct gpujpeg_decoder *decoder;
        uint8_t *input_image;
        void *d_output_image;
        void *output_image;
};

static int decode(const char *input_filename, struct decode_data *d)
{
        if (cudaMalloc(&d->d_output_image, MAX_OUT_LEN) != cudaSuccess) {
            fprintf(stderr, "Cannot allocate output CUDA buffer: %s\n", cudaGetErrorString(cudaGetLastError()));
            return 1;
        }

        if ((d->output_image = malloc(MAX_OUT_LEN)) == NULL) {
            fprintf(stderr, "Cannot allocate output buffer!\n");
            return 1;
        }

        // create decoder
        if ((d->decoder = gpujpeg_decoder_create(0)) == NULL) {
                return 1;
        }

        // load image
        size_t input_image_size = 0;
        if (gpujpeg_image_load_from_file(input_filename, &d->input_image, &input_image_size) != 0) {
                return 1;
        }

        // set decoder default output destination
        struct gpujpeg_decoder_output decoder_output;
        gpujpeg_decoder_output_set_custom_cuda(&decoder_output, d->d_output_image);

        // decompress the image
        if (gpujpeg_decoder_decode(d->decoder, d->input_image, input_image_size, &decoder_output) != 0) {
                return 1;
        }

        // create output file name
        d->out_filename = malloc(strlen(input_filename) + 1);
        strcpy(d->out_filename, input_filename);
        strcpy(strrchr(d->out_filename, '.') + 1, "pnm");

        if (cudaMemcpy(d->output_image, d->d_output_image, decoder_output.data_size, cudaMemcpyDeviceToHost) != cudaSuccess) {
            fprintf(stderr, "Cannot copy from device to host: %s\n", cudaGetErrorString(cudaGetLastError()));
            return 1;
        }

        // write the decoded image
        if ( gpujpeg_image_save_to_file(d->out_filename, d->output_image, decoder_output.data_size,
                                        &decoder_output.param_image) != 0 ) {
            return 1;
        }

        return 0;
}

int main(int argc, char *argv[]) {
        if (!check_params(argc, argv)) {
                usage(argv[0]);
                return 1;
        }

        struct decode_data d = { 0 };
        int rc = decode(argv[1], &d);

        free(d.out_filename);
        gpujpeg_image_destroy(d.input_image);
        if (d.decoder != NULL) {
            gpujpeg_decoder_destroy(d.decoder);
        }
        cudaFree(d.d_output_image);
        free(d.output_image);

        puts(rc == 0 ? "Success\n" : "FAILURE\n");

        return rc;
}

/* vim: set expandtab sw=8: */
