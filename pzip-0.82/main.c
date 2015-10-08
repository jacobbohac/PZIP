
/* Defining _GNU_SOURCE before <string.h>    */
/* gets us the GNU rather than POSIX version */
/* of 'basename', the difference being that  */
/* GNU basename doesn't hack its its input:  */
#define _GNU_SOURCE
#include <string.h>

#include <stdio.h>
#include <ctype.h>
#include "pzip.h"
#include "config.h"
#include "version.h"
#include "inc.h"
#include "crc32.h"
#include "safe.h"
#include "intmath.h"

#define PREAMBLE	(1024)

static const ulong PZIP_MAGIC = 0x70707A32; /* "PPZ2" */
 
static int byte_which_differs( ubyte* buf1, ubyte* buf2 ) {
    int b = 0;
    while (buf1[b] == buf2[b]) {
        b++;
    }
    return b;
}

int verbose = FALSE;

void io_die( const char* plaint, const char* filename ) {
    char buf[ 1023 ];
    sprintf( buf, plaint, filename );
    perror( buf );
    exit( 1 );
} 

void die( const char* plaint ) {
    fputs( plaint, stderr );
    exit( 1 );
} 

/* xxx why not just stat() the file?! */
static ulong file_length( FILE* fp ) {
    long end;
    long start = ftell( fp );

    fseek( fp, 0, SEEK_END );
    end = ftell( fp );

    fseek( fp, start, SEEK_SET );

    return (ulong) end;
}

/* Endian-independent Number IO */

static ulong fget_ul( FILE* fp ) {
    ulong ret;
    ret  = fgetc(fp); ret<<=8;
    ret += fgetc(fp); ret<<=8;
    ret += fgetc(fp); ret<<=8;
    ret += fgetc(fp);
    return ret;
}
static void  fput_ul( ulong v, FILE* fp ) {
    fputc( (v >>24) & 0xFF,  fp );
    fputc( (v >>16) & 0xFF,  fp );
    fputc( (v >> 8) & 0xFF,  fp );
    fputc( (v     ) & 0xFF,  fp );
}



int main(  int argc,   char* argv[] ) {
    char*  in_name  = NULL;
    char*  out_name = NULL;
    ubyte* input_buf;
    ubyte* encode_buf;
    ubyte* decode_buf;
    int    encode_len;
    int    input_len;
    bool   encode_only = FALSE;
    bool   encoding= TRUE;
    FILE*  in_fp    = NULL;
    FILE*  out_fp   = NULL;
    ulong  input_crc  = 0;

    if (argc < 2) {
	fprintf(stderr, "pzip version %.2f\n", VERSION );
	fprintf(stderr, "Usage : pzip [options] <in> [out]\n" );
	fprintf(stderr, "options :\n" );
	fprintf(stderr, " -e  : encode only [vs also decode and compare]\n");
	fprintf(stderr, " -v  : verbose output during run\n");
	exit(1);
    }

    ++argv;
    --argc;


    /* Process options: */
    while (argc > 0) {
        char*  str = *argv++;
        argc--;

        if (*str == '-') {
            str++;

            switch (*str++) {

            case 'e':
                encode_only = TRUE;
                break;

            case 'v':
                ++verbose;
                break;

            default:
                fprintf(stderr, "unknown option '-%c' skipped\n", str[-1] );
                break;
            }

        } else {

            if        (!in_name)  { in_name  = str;
            } else if (!out_name) { out_name = str;
            } else {
                fprintf(stderr, "extraneous parameter '%s' ignored.\n", str );
            }
        }
    }

    intmath_init();

    in_fp = fopen( in_name, "r" );
    if (!in_fp)   io_die( "main.c:main(): Couldn't open input file '%s'", in_name );

    input_len = file_length(in_fp);

    if (out_name) {
        out_fp = fopen( out_name, "w" );
        if (!out_fp)   io_die( "main.c:main(): Couldn't open output file '%s'", out_name );
    }

    encoding = TRUE;

    if (out_fp) {

        /* Is in_fp compressed? */
        ulong tag = fget_ul( in_fp );
        if (tag == PZIP_MAGIC) {
            /* It is packed: */
            input_len = fget_ul( in_fp );
            input_crc = fget_ul( in_fp );
            encoding = FALSE;
        } else {
            /* Not packed: */
            fseek( in_fp, 0, SEEK_SET );
            fput_ul( PZIP_MAGIC, out_fp );
            fput_ul( input_len, out_fp );
        }
    }

    input_buf = safe_Malloc( input_len + 1024 + PREAMBLE );
    memset( input_buf, ' ', PREAMBLE );
    input_buf += PREAMBLE;

    if (encoding) {
        fread( input_buf, 1, input_len, in_fp );
        fclose(in_fp);
        in_fp = NULL;

        input_crc = crc32_Compute_Checksum( input_buf, input_len );

        if (out_fp) { fput_ul(input_crc,out_fp); }
    }

    decode_buf = safe_Malloc( input_len + 1024 + PREAMBLE );
    memset( decode_buf, ' ', PREAMBLE );
    decode_buf += PREAMBLE;

    encode_buf = safe_Malloc( input_len*2 + 65536 + PREAMBLE );
    memset( encode_buf, 0, PREAMBLE );
    encode_buf += PREAMBLE;

    if (encoding) {
        encode_len = pzip_Encode( input_buf, input_len, encode_buf );
        if (verbose) {
            fprintf(stderr,
                "%-20s : %8d -> %8d = %1.3f bpc\n",
                basename(in_name), input_len, encode_len, encode_len * 8.0 / (double) input_len
            );
        }
    } else {
        /* 12 bytes of header: */
        encode_len = file_length(in_fp) - 12;
        fseek( in_fp, 12, SEEK_SET );
        fread( encode_buf, 1, encode_len, in_fp );
        fclose(in_fp);
        in_fp = NULL; 
    }

    if (!encode_only) {

        pzip_Decode( decode_buf, input_len, encode_buf );

        /* Sanity check --- see if decoded CRC is correct: */
        {   ulong decode_crc = crc32_Compute_Checksum( decode_buf, input_len );
            if (decode_crc != input_crc)	{
                fprintf(stderr, "***** FILE CORRUPTED!  CRC32 should be %08lX but actually is %08lX\n", input_crc, decode_crc );
            }
        }
    }

    /* Compare input_buf and decode_buf if we have them both: */
    if (encoding && !encode_only) {
        if (memcmp( decode_buf, input_buf, input_len )) {
            fprintf(stderr,
                "***** Decode failed: %d th bytes differ\n",
                byte_which_differs( decode_buf, input_buf )
            );
        }
    }

    if (out_fp) {
        if (encoding) fwrite( encode_buf, 1, encode_len, out_fp );
        else          fwrite( decode_buf, 1, input_len,    out_fp );

        fclose( out_fp );
        out_fp = NULL;
    }

    exit( 0 );
}
