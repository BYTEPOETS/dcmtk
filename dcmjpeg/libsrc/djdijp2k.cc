
#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmjpeg/djdijp2k.h"
#include "dcmtk/dcmjpeg/djcparam.h"
#include "dcmtk/ofstd/ofconsol.h"
#include "dcmtk/dcmjpeg/OPJSupport.h"

#include <sys/types.h>


#define INCLUDE_CSTDIO
#define INCLUDE_CSETJMP
#include "dcmtk/ofstd/ofstdinc.h"

// These two macros are re-defined in the IJG header files.
// We undefine them here and hope that IJG's configure has
// come to the same conclusion that we have...
#ifdef HAVE_STDLIB_H
#undef HAVE_STDLIB_H
#endif
#ifdef HAVE_STDDEF_H
#undef HAVE_STDDEF_H
#endif


BEGIN_EXTERN_C
#define boolean ijg_boolean
#include "jpeglib16.h"
#include "jerror16.h"
#undef boolean

// disable any preprocessor magic the IJG library might be doing with the "const" keyword
#ifdef const
#undef const
#endif


//#include "openjpeg.h"
/**
sample error callback expecting a FILE* client object
*/
static void error_callback(const char *msg, void *a)
{
//	printf( "%s\r\r", msg);
}
/**
sample warning callback expecting a FILE* client object
*/
static void warning_callback(const char *msg, void *a)
{
//	printf( "%s\r\r", msg);
}

/**
sample debug callback expecting no client object
*/
static void info_callback(const char *msg, void *a)
{
//	printf( "%s\r\r", msg);
}

static inline int int_ceildivpow2(int a, int b) {
	return (a + (1 << b) - 1) >> b;
}



DJDecompressJP2k::DJDecompressJP2k(const DJCodecParameter& cp, OFBool isYBR)
: DJDecoder()
, cparam(&cp)
, cinfo(NULL)
, suspension(0)
, jsampBuffer(NULL)
, dicomPhotometricInterpretationIsYCbCr(isYBR)
, decompressedColorModel(EPI_Unknown)
{
}

DJDecompressJP2k::~DJDecompressJP2k()
{
  cleanup();
}


OFCondition DJDecompressJP2k::init()
{
  // everything OK
  return EC_Normal;
}


void DJDecompressJP2k::cleanup()
{
}


OFCondition DJDecompressJP2k::decode(
  Uint8 *compressedFrameBuffer,
  Uint32 compressedFrameBufferSize,
  Uint8 *uncompressedFrameBuffer,
  Uint32 uncompressedFrameBufferSize,
  OFBool isSigned)
{
	OPJSupport* supp = new OPJSupport();
	int colorModel;
	long uncompressedSize = 0;
	void* res = supp->decompressJPEG2KWithBuffer(uncompressedFrameBuffer, compressedFrameBuffer, compressedFrameBufferSize,&uncompressedSize, &colorModel);
	if (!res)
		return EC_MemoryExhausted;
	if( colorModel == 1)
		decompressedColorModel = (EP_Interpretation) EPI_RGB;
	delete supp;
	return EC_Normal;
}

void DJDecompressJP2k::outputMessage() const
{

}
}
