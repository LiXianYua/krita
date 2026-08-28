/*
 *  SPDX-FileCopyrightText: 2009 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.1-only
 */

#ifndef _JP2_CONVERTER_H_
#define _JP2_CONVERTER_H_

#include <stdio.h>
#include <string>

#include "kis_types.h"
#include "kis_global.h"
#include "kis_annotation.h"
#include <KisImportExportErrorCode.h>

class KisDocument;
class PkStream;
class PkString;

struct JP2ConvertOptions {
	int rate;
	int numberresolution;
};

class JP2Converter {
public:
	JP2Converter(KisDocument *doc);
	virtual ~JP2Converter();
public:
	KisImportExportErrorCode buildImage(PkStream *stream);
	KisImportExportErrorCode buildFile(const PkString &filename,
			KisPaintLayerSP layer, const JP2ConvertOptions &options);
	/**
	 * Retrieve the constructed image
	 */
	KisImageWSP image();
	void addErrorString(const std::string&  str);
	void addWarningString(const std::string&  str);
	void addInfoString(const std::string&  str);

private:
	int infileFormat(PkStream *stream);
public:
	virtual void cancel();
private:
	KisImageSP m_image;
	KisDocument *m_doc;
	bool m_stop;
	std::string err;
	std::string warn;
};

#endif
