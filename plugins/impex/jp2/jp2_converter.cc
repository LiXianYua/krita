/*
 *  SPDX-FileCopyrightText: 2019 Aaron Boxer <boxerab@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.1-only
 */

#include "jp2_converter.h"
#include "jp2_validation.h"

#include <openjpeg.h>
#include <PkStream.h>
#include <PkString.h>

#include <KoColorSpaceRegistry.h>
#include <KoColorSpaceTraits.h>
#include <KoColorSpaceConstants.h>
#include <KisImportExportManager.h>
#include <KoColorSpace.h>
#include <KoColorModelStandardIds.h>

#include <KisDocument.h>
#include <kis_image.h>
#include <kis_group_layer.h>
#include <kis_paint_layer.h>
#include <kis_paint_device.h>
#include <kis_transaction.h>
#include "kis_iterator_ng.h"
#include <PkThread.h>
#include <plugins/impex/xcf/3rdparty/xcftools/xcftools.h>

#include <iostream>
#include <sstream>
#include <cstring>
#include <list> 
#include <utility>

#define J2K_CFMT 0
#define JP2_CFMT 1

JP2Converter::JP2Converter(KisDocument *doc) {
	m_doc = doc;
	m_stop = false;
}

JP2Converter::~JP2Converter() {
}

/**
 * sample error callback expecting a FILE* client object
 * */
static void error_callback(const char *msg, void *client_data) {
	JP2Converter *converter = (JP2Converter*) client_data;
	converter->addErrorString(msg);
}

/**
 * sample warning callback expecting a FILE* client object
 * */
static void warning_callback(const char *msg, void *client_data) {
	JP2Converter *converter = (JP2Converter*) client_data;
	converter->addWarningString(msg);
}

/**
 * sample debug callback expecting no client object
 * */
static void info_callback(const char *msg, void *client_data) {
	JP2Converter *converter = (JP2Converter*) client_data;
	converter->addInfoString(msg);
}

#define JP2_RFC3745_MAGIC 	 "\x00\x00\x00\x0c\x6a\x50\x20\x20\x0d\x0a\x87\x0a"
#define JP2_MAGIC 			 "\x0d\x0a\x87\x0a"
#define J2K_CODESTREAM_MAGIC "\xff\x4f\xff\x51"

int JP2Converter::infileFormat(PkStream *stream) {
	unsigned char buf[12] = {};
	if (!stream || stream->peek(reinterpret_cast<char *>(buf), sizeof(buf)) != sizeof(buf)) {
		return -1;
	}
	if (memcmp(buf, JP2_RFC3745_MAGIC, 12) == 0
			|| memcmp(buf, JP2_MAGIC, 4) == 0) {
		return JP2_CFMT;
	} else if (memcmp(buf, J2K_CODESTREAM_MAGIC, 4) == 0) {
		return J2K_CFMT;
	}
	return -1;
}

static OPJ_SIZE_T readStream(void *buffer, OPJ_SIZE_T size, void *userData)
{
	auto *stream = static_cast<PkStream *>(userData);
	const auto count = stream->read(static_cast<char *>(buffer), static_cast<PkStream::pk_int64>(size));
	return count > 0 ? static_cast<OPJ_SIZE_T>(count) : static_cast<OPJ_SIZE_T>(-1);
}

static OPJ_OFF_T skipStream(OPJ_OFF_T count, void *userData)
{
	auto *stream = static_cast<PkStream *>(userData);
	const auto oldPosition = stream->pos();
	return stream->seek(oldPosition + count) ? count : static_cast<OPJ_OFF_T>(-1);
}

static OPJ_BOOL seekStream(OPJ_OFF_T position, void *userData)
{
	return static_cast<PkStream *>(userData)->seek(position) ? OPJ_TRUE : OPJ_FALSE;
}

static opj_stream_t *createOpenJpegStream(PkStream *stream)
{
	opj_stream_t *result = opj_stream_create(64 * 1024, OPJ_TRUE);
	if (!result) {
		return nullptr;
	}
	opj_stream_set_user_data(result, stream, nullptr);
	opj_stream_set_user_data_length(result, static_cast<OPJ_UINT64>(stream->size()));
	opj_stream_set_read_function(result, readStream);
	opj_stream_set_skip_function(result, skipStream);
	opj_stream_set_seek_function(result, seekStream);
	return result;
}

KisImportExportErrorCode JP2Converter::buildImage(PkStream *input) {
	KisImportExportErrorCode res = ImportExportCodes::OK;
	opj_codec_t *l_codec = 0;
	opj_dparameters_t parameters;
	bool hasColorSpaceInfo = false;
	opj_stream_t *l_stream = NULL;
	opj_image_t *image = NULL;
	std::size_t pos = 0;
	KisHLineIteratorSP it = NULL;
	unsigned int numComponents = 0;
	unsigned int precision = 0;
	const KoColorSpace *colorSpace = 0;
	PkVector<int> channelorder;
	KisPaintLayerSP layer;
	bool isSigned = false;
	int32_t signedCorrection = 0;
	uint32_t w=0, h=0;
	Jp2ValidatedImage validatedImage{};

	// decompression parameters
	opj_set_default_decoder_parameters(&parameters);
	// Determine the type
	parameters.decod_format = infileFormat(input);
	if (parameters.decod_format == -1) {
		addErrorString("Not a JPEG 2000 file.");
		res = ImportExportCodes::FileFormatIncorrect;
		goto beach;
	}

	// Decode the file
	/* get a decoder handle */
	switch (parameters.decod_format) {
	case J2K_CFMT: {
		l_codec = opj_create_decompress(OPJ_CODEC_J2K);
		break;
	}
	case JP2_CFMT: {
		l_codec = opj_create_decompress(OPJ_CODEC_JP2);
		hasColorSpaceInfo = true;
		break;
	}
	}
	if (!l_codec) {
		addErrorString("Failed to create the decoder");
		res = ImportExportCodes::InternalError;
		goto beach;
	}

	opj_codec_set_threads( l_codec,PkThread::idealThreadCount() );

	/* setup the decoder decoding parameters using user parameters */
	opj_setup_decoder(l_codec, &parameters);

	l_stream = createOpenJpegStream(input);
	if (!l_stream) {
		addErrorString("Failed to create the stream");
		res = ImportExportCodes::ErrorWhileReading;
		goto beach;
	}

	// Setup an event handling
	opj_set_info_handler(l_codec, info_callback, this);
	opj_set_error_handler(l_codec, error_callback, this);
	opj_set_warning_handler(l_codec, warning_callback, this);

	if (!opj_read_header(l_stream, l_codec, &image)) {
		addErrorString("Failed to read the header");
		res = ImportExportCodes::ErrorWhileReading;
		goto beach;
	}

	/* Get the decoded image */
	if (!(opj_decode(l_codec, l_stream, image)
			&& opj_end_decompress(l_codec, l_stream))) {
		addErrorString("Failed to decode image");
		res = ImportExportCodes::ErrorWhileReading;
		goto beach;
	}

	// Validate the complete decoded layout before using any component pointer.
	if (!validateJp2Image(*image, validatedImage)) {
		addErrorString("Invalid or unsupported JPEG 2000 component layout");
		res = ImportExportCodes::FormatFeaturesUnsupported;
		goto beach;
	}
	numComponents = image->numcomps;
	precision = validatedImage.precision;
	isSigned = validatedImage.isSigned;
	if (isSigned)
		signedCorrection = 1 << (precision - 1);

	dbgFile
	<< "Image has " << numComponents << " numComponents and a bit depth of "
			<< precision << " for color space " << image->color_space;
	channelorder = PkVector<int>(numComponents);
	if (!hasColorSpaceInfo) {
		if (numComponents == 3) {
			image->color_space = OPJ_CLRSPC_SRGB;
		} else if (numComponents == 1) {
			image->color_space = OPJ_CLRSPC_GRAY;
		}
	}
	switch (image->color_space) {
	case OPJ_CLRSPC_UNKNOWN:
	case OPJ_CLRSPC_UNSPECIFIED:
		break;
	case OPJ_CLRSPC_SRGB: {
		if (precision == 16 || precision == 12) {
			colorSpace = KoColorSpaceRegistry::instance()->rgb16();
		} else if (precision == 8) {
			colorSpace = KoColorSpaceRegistry::instance()->rgb8();
		}
		if (numComponents != 3) {
			std::ostringstream buffer;
			buffer << "sRGB: number of numComponents " << numComponents
					<< " does not equal 3";
			addErrorString(buffer.str());
			res = ImportExportCodes::FormatFeaturesUnsupported;
			goto beach;
		}
		channelorder[0] = KoBgrU16Traits::red_pos;
		channelorder[1] = KoBgrU16Traits::green_pos;
		channelorder[2] = KoBgrU16Traits::blue_pos;
		break;
	}
	case OPJ_CLRSPC_GRAY: {
		if (precision == 16 || precision == 12) {
			colorSpace = KoColorSpaceRegistry::instance()->colorSpace(
					GrayAColorModelID.id(), Integer16BitsColorDepthID.id(), "");
		} else if (precision == 8) {
			colorSpace = KoColorSpaceRegistry::instance()->colorSpace(
					GrayAColorModelID.id(), Integer8BitsColorDepthID.id(), "");
		}
		if (numComponents != 1) {
			std::ostringstream buffer;
			buffer << "Grayscale: number of numComponents " << numComponents
					<< " greater than 1";
			addErrorString(buffer.str());
			res = ImportExportCodes::FormatFeaturesUnsupported;
			goto beach;
		}
		channelorder[0] = 0;
		break;
	}
	case OPJ_CLRSPC_SYCC:
		addErrorString("YUV color space not supported");
		res = ImportExportCodes::FormatColorSpaceUnsupported;
		goto beach;
		break;
	case OPJ_CLRSPC_EYCC:
		addErrorString("eYCC color space not supported");
		res = ImportExportCodes::FormatColorSpaceUnsupported;
		goto beach;
		break;
	case OPJ_CLRSPC_CMYK:
		addErrorString("CMYK color space not supported");
		res = ImportExportCodes::FormatColorSpaceUnsupported;
		goto beach;
		break;
	default:
		break;
	}

	if (!colorSpace) {
		addErrorString("No color space found for image");
		res = ImportExportCodes::FormatColorSpaceUnsupported;
		goto beach;
	}

	// Create the image
	w = validatedImage.width;
	h = validatedImage.height;
	if (m_image == 0) {
		m_image = new KisImage(m_doc->createUndoStore(), static_cast<int>(w), static_cast<int>(h),
				colorSpace, "built image");
	}

	// Create the layer
	layer = new KisPaintLayer(m_image, m_image->nextLayerName(),
			OPACITY_OPAQUE_U8);
	m_image->addNode(layer);

	// Set the data
	it = layer->paintDevice()->createHLineIteratorNG(0, 0, w);
	for (OPJ_UINT32 v = 0; v < h; ++v) {
		if (precision == 16 || precision == 12) {
			do {
				if (pos >= validatedImage.pixelCount) {
					res = ImportExportCodes::FileFormatIncorrect;
					goto beach;
				}
				quint16 *px = reinterpret_cast<quint16*>(it->rawData());
				for (uint32_t i = 0; i < numComponents; ++i) {
					px[channelorder[i]] = image->comps[i].data[pos]
							+ signedCorrection;
				}
				colorSpace->setOpacity(it->rawData(), OPACITY_OPAQUE_U8, 1);
				++pos;

			} while (it->nextPixel());
		} else if (precision == 8) {
			do {
				if (pos >= validatedImage.pixelCount) {
					res = ImportExportCodes::FileFormatIncorrect;
					goto beach;
				}
				quint8 *px = it->rawData();
				for (uint32_t i = 0; i < numComponents; ++i) {
					px[channelorder[i]] = image->comps[i].data[pos]
							+ signedCorrection;
				}
				colorSpace->setOpacity(px, OPACITY_OPAQUE_U8, 1);
				++pos;

			} while (it->nextPixel());
		}
		it->nextRow();
	}
	if (pos != validatedImage.pixelCount) {
		res = ImportExportCodes::FileFormatIncorrect;
		goto beach;
	}

beach:
	if (l_stream)
		opj_stream_destroy(l_stream);
	if (l_codec)
		opj_destroy_codec(l_codec);
	if (image)
		opj_image_destroy(image);
	if (!err.empty())
		m_doc->setErrorMessage(PkString(err.c_str()));
	if (!warn.empty())
		m_doc->setWarningMessage(PkString(warn.c_str()));
	return res;
}

KisImageWSP JP2Converter::image() {
	return m_image;
}

KisImportExportErrorCode JP2Converter::buildFile(const PkString &filename,
		KisPaintLayerSP layer, const JP2ConvertOptions &options) {
	(void) layer;
	(void) filename;
	(void) options;
	return ImportExportCodes::Failure;
}

void JP2Converter::cancel() {
	m_stop = true;
}

void JP2Converter::addWarningString(const std::string &str) {
	if (!warn.empty())
		warn += "\n";
	warn += str;
}
void JP2Converter::addInfoString(const std::string &str) {
	dbgFile
	<< str.c_str();
}

void JP2Converter::addErrorString(const std::string &str) {
	if (!err.empty())
		err += "\n";
	err += str;
}
