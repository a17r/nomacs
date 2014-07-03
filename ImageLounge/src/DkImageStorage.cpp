/*******************************************************************************************************
 DkImageStorage.cpp
 Created on:	12.07.2013
 
 nomacs is a fast and small image viewer with the capability of synchronizing multiple instances
 
 Copyright (C) 2011-2013 Markus Diem <markus@nomacs.org>
 Copyright (C) 2011-2013 Stefan Fiel <stefan@nomacs.org>
 Copyright (C) 2011-2013 Florian Kleber <florian@nomacs.org>

 This file is part of nomacs.

 nomacs is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 nomacs is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 *******************************************************************************************************/

#include "DkImageStorage.h"
#include "DkSettings.h"
#include "DkTimer.h"

namespace nmc {

// DkImage --------------------------------------------------------------------
#ifdef WIN32

// this function is copied from Qt 4.8.5 qpixmap_win.cpp since Qt removed the conversion from
// the QPixmap class in Qt5 and we are not interested in more Qt5/4 conversions. In addition,
// we would need another module int Qt 5
QImage DkImage::fromWinHBITMAP(HDC hdc, HBITMAP bitmap, int w, int h) {
	
	BITMAPINFO bmi;
	memset(&bmi, 0, sizeof(bmi));
	bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth       = w;
	bmi.bmiHeader.biHeight      = -h;
	bmi.bmiHeader.biPlanes      = 1;
	bmi.bmiHeader.biBitCount    = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biSizeImage   = w * h * 4;

	QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
	if (image.isNull())
		return image;

	// Get bitmap bits
	uchar *data = (uchar *) malloc(bmi.bmiHeader.biSizeImage); // is it cool to use malloc here?

	if (GetDIBits(hdc, bitmap, 0, h, data, &bmi, DIB_RGB_COLORS)) {
		// Create image and copy data into image.
		for (int y=0; y<h; ++y) {
			void *dest = (void *) image.scanLine(y);
			void *src = data + y * image.bytesPerLine();
			memcpy(dest, src, image.bytesPerLine());
		}
	} else {
		qWarning("qt_fromWinHBITMAP(), failed to get bitmap bits");
	}
	free(data);

	return image;
}

// this function is copied from Qt 4.8.5 qpixmap_win.cpp since Qt removed the conversion from
// the QPixmap class in Qt5 and we are not interested in more Qt5/4 conversions. In addition,
// we would need another module int Qt 5
QPixmap DkImage::fromWinHICON(HICON icon) {
	
	bool foundAlpha = false;
	HDC screenDevice = GetDC(0);
	HDC hdc = CreateCompatibleDC(screenDevice);
	ReleaseDC(0, screenDevice);

	ICONINFO iconinfo;
	bool result = GetIconInfo(icon, &iconinfo); //x and y Hotspot describes the icon center
	if (!result)
		qWarning("QPixmap::fromWinHICON(), failed to GetIconInfo()");

	int w = iconinfo.xHotspot * 2;
	int h = iconinfo.yHotspot * 2;

	BITMAPINFOHEADER bitmapInfo;
	bitmapInfo.biSize        = sizeof(BITMAPINFOHEADER);
	bitmapInfo.biWidth       = w;
	bitmapInfo.biHeight      = h;
	bitmapInfo.biPlanes      = 1;
	bitmapInfo.biBitCount    = 32;
	bitmapInfo.biCompression = BI_RGB;
	bitmapInfo.biSizeImage   = 0;
	bitmapInfo.biXPelsPerMeter = 0;
	bitmapInfo.biYPelsPerMeter = 0;
	bitmapInfo.biClrUsed       = 0;
	bitmapInfo.biClrImportant  = 0;
	DWORD* bits;

	HBITMAP winBitmap = CreateDIBSection(hdc, (BITMAPINFO*)&bitmapInfo, DIB_RGB_COLORS, (VOID**)&bits, NULL, 0);
	HGDIOBJ oldhdc = (HBITMAP)SelectObject(hdc, winBitmap);
	DrawIconEx( hdc, 0, 0, icon, iconinfo.xHotspot * 2, iconinfo.yHotspot * 2, 0, 0, DI_NORMAL);
	QImage image = fromWinHBITMAP(hdc, winBitmap, w, h);

	for (int y = 0 ; y < h && !foundAlpha ; y++) {
		QRgb *scanLine= reinterpret_cast<QRgb *>(image.scanLine(y));
		for (int x = 0; x < w ; x++) {
			if (qAlpha(scanLine[x]) != 0) {
				foundAlpha = true;
				break;
			}
		}
	}
	if (!foundAlpha) {
		//If no alpha was found, we use the mask to set alpha values
		DrawIconEx( hdc, 0, 0, icon, w, h, 0, 0, DI_MASK);
		QImage mask = fromWinHBITMAP(hdc, winBitmap, w, h);

		for (int y = 0 ; y < h ; y++){
			QRgb *scanlineImage = reinterpret_cast<QRgb *>(image.scanLine(y));
			QRgb *scanlineMask = mask.isNull() ? 0 : reinterpret_cast<QRgb *>(mask.scanLine(y));
			for (int x = 0; x < w ; x++){
				if (scanlineMask && qRed(scanlineMask[x]) != 0)
					scanlineImage[x] = 0; //mask out this pixel
				else
					scanlineImage[x] |= 0xff000000; // set the alpha channel to 255
			}
		}
	}
	//dispose resources created by iconinfo call
	DeleteObject(iconinfo.hbmMask);
	DeleteObject(iconinfo.hbmColor);

	SelectObject(hdc, oldhdc); //restore state
	DeleteObject(winBitmap);
	DeleteDC(hdc);
	return QPixmap::fromImage(image);
}
#endif

QImage DkImage::resizeImage(const QImage img, const QSize& newSize, float factor /* = 1.0f */, int interpolation /* = ipl_cubic */, bool correctGamma /* = true */) {

	QSize nSize = newSize;

	// nothing to do
	if (img.size() == nSize && factor == 1.0f)
		return img;

	if (factor != 1.0f)
		nSize = QSize(img.width()*factor, img.height()*factor);

	if (nSize.width() < 1 || nSize.height() < 1) {
		return QImage();
	}

	Qt::TransformationMode iplQt;
	switch(interpolation) {
	case ipl_nearest:	
	case ipl_area:		iplQt = Qt::FastTransformation; break;
	case ipl_linear:	
	case ipl_cubic:		
	case ipl_lanczos:	iplQt = Qt::SmoothTransformation; break;
	}
#ifdef WITH_OPENCV

	int ipl = CV_INTER_CUBIC;
	switch(interpolation) {
	case ipl_nearest:	ipl = CV_INTER_NN; break;
	case ipl_area:		ipl = CV_INTER_AREA; break;
	case ipl_linear:	ipl = CV_INTER_LINEAR; break;
	case ipl_cubic:		ipl = CV_INTER_CUBIC; break;
#ifdef DISABLE_LANCZOS
	case ipl_lanczos:	ipl = CV_INTER_CUBIC; break;
#else
	case ipl_lanczos:	ipl = CV_INTER_LANCZOS4; break;
#endif
	}


	try {
		QImage qImg = img.copy();
		
		if (correctGamma)
			DkImage::gammaToLinear(qImg);
		Mat resizeImage = DkImage::qImage2Mat(qImg);

		// is the image convertible?
		if (resizeImage.empty()) {
			qImg = qImg.scaled(newSize, Qt::IgnoreAspectRatio, iplQt);
		}
		else {

			Mat tmp;
			cv::resize(resizeImage, tmp, cv::Size(nSize.width(), nSize.height()), 0, 0, ipl);
			resizeImage = tmp;
			qImg = DkImage::mat2QImage(resizeImage);
		}

		if (correctGamma)
			DkImage::linearToGamma(qImg);

		if (!img.colorTable().isEmpty())
			qImg.setColorTable(img.colorTable());

		return qImg;

	}catch (std::exception se) {

		return QImage();
	}

#else

	QImage qImg = img.copy();
	
	if (correctGamma)
		DkImage::gammaToLinear(qImg);
	qImg.scaled(nSize, Qt::IgnoreAspectRatio, iplQt);
	
	if (correctGamma)
		DkImage::linearToGamma(qImg);
	return qImg;
#endif
}
	
bool DkImage::alphaChannelUsed(const QImage& img) {

	if (img.format() != QImage::Format_ARGB32 && img.format() != QImage::Format_ARGB32_Premultiplied)
		return false;

	// number of used bytes per line
	int bpl = (img.width() * img.depth() + 7) / 8;
	int pad = img.bytesPerLine() - bpl;
	const uchar* ptr = img.bits();

	for (int rIdx = 0; rIdx < img.height(); rIdx++) {

		for (int cIdx = 0; cIdx < bpl; cIdx++, ptr++) {

			if (cIdx % 4 == 3 && *ptr != 255)
				return true;
		}

		ptr += pad;
	}

	return false;
}

QVector<uchar> DkImage::getLinear2GammaTable() {

	QVector<uchar> gammaTable;
	double a = 0.055;

	for (int idx = 0; idx < 256; idx++) {

		double i = idx/255.0;
		if (i <= 0.0031308) {
			gammaTable.append(qRound(i*12.92*255.0));
		}
		else {
			gammaTable.append(qRound(((1+a)*std::pow(i,1/2.4)-a)*255.0));
		}
	}

	return gammaTable;
}

QVector<uchar> DkImage::getGamma2LinearTable() {

	// the formula should be:
	// i = px/255
	// i <= 0.04045 -> i/12.92
	// i > 0.04045 -> (i+0.055)/(1+0.055)^2.4

	qDebug() << "gamma2Linear: ";
	QVector<uchar> gammaTable;
	double a = 0.055;

	for (int idx = 0; idx < 256; idx++) {

		double i = idx/255.0;
		if (i <= 0.04045) {
			gammaTable.append(qRound(i/12.92*255.0));
		}
		else {
			gammaTable.append(std::pow((i+a)/(1+a),2.4)*255 > 0 ? std::pow((i+a)/(1+a),2.4)*255 : 0);
		}
	}

	return gammaTable;
}

void DkImage::gammaToLinear(QImage& img) {

	QVector<uchar> gt = getGamma2LinearTable();
	mapGammaTable(img, gt);
}

void DkImage::linearToGamma(QImage& img) {

	QVector<uchar> gt = getLinear2GammaTable();
	mapGammaTable(img, gt);
}

void DkImage::mapGammaTable(QImage& img, const QVector<uchar>& gammaTable) {

	DkTimer dt;

	// number of bytes per line used
	int bpl = (img.width() * img.depth() + 7) / 8;
	int pad = img.bytesPerLine() - bpl;

	//int channels = (img.hasAlphaChannel() || img.format() == QImage::Format_RGB32) ? 4 : 3;

	uchar* mPtr = img.bits();

	for (int rIdx = 0; rIdx < img.height(); rIdx++) {

		for (int cIdx = 0; cIdx < bpl; cIdx++, mPtr++) {

			if (*mPtr < 0 || *mPtr > 255)
				qDebug() << "WRONG VALUE: " << *mPtr;

			if ((int)gammaTable[*mPtr] < 0 || (int)gammaTable[*mPtr] > 255)
				qDebug() << "WRONG VALUE: " << *mPtr;


			*mPtr = gammaTable[*mPtr];
		}
		mPtr += pad;
	}

	qDebug() << "gamma computation takes: " << dt.getTotal();
}


QImage DkImage::normImage(const QImage& img) {

	QImage imgN = img.copy();
	normImage(imgN);

	return imgN;
}

bool DkImage::normImage(QImage& img) {

	uchar maxVal = 0;
	uchar minVal = 255;

	// number of used bytes per line
	int bpl = (img.width() * img.depth() + 7) / 8;
	int pad = img.bytesPerLine() - bpl;
	uchar* mPtr = img.bits();
	bool hasAlpha = img.hasAlphaChannel() || img.format() == QImage::Format_RGB32;

	for (int rIdx = 0; rIdx < img.height(); rIdx++) {
		
		for (int cIdx = 0; cIdx < bpl; cIdx++, mPtr++) {
			
			if (hasAlpha && cIdx % 4 == 3)
				continue;

			if (*mPtr > maxVal)
				maxVal = *mPtr;
			if (*mPtr < minVal)
				minVal = *mPtr;
		}
		
		mPtr += pad;
	}

	if (minVal == 0 && maxVal == 255 || maxVal-minVal == 0)
		return false;

	uchar* ptr = img.bits();
	
	for (int rIdx = 0; rIdx < img.height(); rIdx++) {
	
		for (int cIdx = 0; cIdx < bpl; cIdx++, ptr++) {

			if (hasAlpha && cIdx % 4 == 3)
				continue;

			*ptr = qRound(255.0f*(*ptr-minVal)/(maxVal-minVal));
		}
		
		ptr += pad;
	}

	return true;

}

QImage DkImage::autoAdjustImage(const QImage& img) {

	QImage imgA = img.copy();
	autoAdjustImage(imgA);

	return imgA;
}

bool DkImage::autoAdjustImage(QImage& img) {

	//return DkImage::unsharpMask(img, 30.0f, 1.5f);

	DkTimer dt;
	qDebug() << "[Auto Adjust] image format: " << img.format();

	// for grayscale image - normalize is the same
	if (img.format() <= QImage::Format_Indexed8) {
		qDebug() << "[Auto Adjust] Grayscale - switching to Normalize: " << img.format();
		return normImage(img);
	}
	else if (img.format() != QImage::Format_ARGB32 && img.format() != QImage::Format_ARGB32_Premultiplied && 
		img.format() != QImage::Format_RGB32 && img.format() != QImage::Format_RGB888) {
		qDebug() << "[Auto Adjust] Format not supported: " << img.format();
		return false;
	}

	int channels = (img.hasAlphaChannel() || img.format() == QImage::Format_RGB32) ? 4 : 3;

	uchar maxR = 0,		maxG = 0,	maxB = 0;
	uchar minR = 255,	minG = 255, minB = 255;

	// number of bytes per line used
	int bpl = (img.width() * img.depth() + 7) / 8;
	int pad = img.bytesPerLine() - bpl;

	uchar* mPtr = img.bits();
	uchar r,g,b;

	int histR[256] = {0};
	int histG[256] = {0};
	int histB[256] = {0};

	for (int rIdx = 0; rIdx < img.height(); rIdx++) {

		for (int cIdx = 0; cIdx < bpl; ) {

			r = *mPtr; mPtr++;
			g = *mPtr; mPtr++;
			b = *mPtr; mPtr++;
			cIdx += 3;

			if (r > maxR)	maxR = r;
			if (r < minR)	minR = r;

			if (g > maxG)	maxG = g;
			if (g < minG)	minG = g;

			if (b > maxB)	maxB = b;
			if (b < minB)	minB = b;

			histR[r]++;
			histG[g]++;
			histB[b]++;


			// ?? strange but I would expect the alpha channel to be the first (big endian?)
			if (channels == 4) {
				mPtr++;
				cIdx++;
			}

		}
		mPtr += pad;
	}

	QColor ignoreChannel;
	bool ignoreR = maxR-minR == 0 || maxR-minR == 255;
	bool ignoreG = maxR-minR == 0 || maxG-minG == 255;
	bool ignoreB = maxR-minR == 0 || maxB-minB == 255;

	uchar* ptr = img.bits();

	if (ignoreR) {
		maxR = findHistPeak(histR);
		ignoreR = maxR-minR == 0 || maxR-minR == 255;
	}
	if (ignoreG) {
		maxG = findHistPeak(histG);
		ignoreG = maxG-minG == 0 || maxG-minG == 255;
	}
	if (ignoreB) {
		maxB = findHistPeak(histB);
		ignoreB = maxB-minB == 0 || maxB-minB == 255;
	}

	//qDebug() << "red max: " << maxR << " min: " << minR << " ignored: " << ignoreR;
	//qDebug() << "green max: " << maxG << " min: " << minG << " ignored: " << ignoreG;
	//qDebug() << "blue max: " << maxB << " min: " << minB << " ignored: " << ignoreB;
	//qDebug() << "computed in: " << dt.getTotal();

	if (ignoreR && ignoreG && ignoreB) {
		qDebug() << "[Auto Adjust] There is no need to adjust the image";
		return false;
	}

	for (int rIdx = 0; rIdx < img.height(); rIdx++) {

		for (int cIdx = 0; cIdx < bpl; ) {

			// don't check values - speed (but you see under-/overflows anyway)
			if (!ignoreR && *ptr < maxR)
				*ptr = qRound(255.0f*((float)*ptr-minR)/(maxR-minR));
			else if (!ignoreR)
				*ptr = 255;

			ptr++;
			cIdx++;

			if (!ignoreG && *ptr < maxG)
				*ptr = qRound(255.0f*((float)*ptr-minG)/(maxG-minG));
			else if (!ignoreG)
				*ptr = 255;

			ptr++;
			cIdx++;

			if (!ignoreB && *ptr < maxB)
				*ptr = qRound(255.0f*((float)*ptr-minB)/(maxB-minB));
			else if (!ignoreB)
				*ptr = 255;
			ptr++;
			cIdx++;

			if (channels == 4) {
				ptr++;
				cIdx++;
			}

		}
		ptr += pad;
	}

	qDebug() << "[Auto Adjust] image adjusted in: " << dt.getTotal();
	
	return true;
}

uchar DkImage::findHistPeak(const int* hist, float quantile) {

	int histArea = 0;

	for (int idx = 0; idx < 256; idx++)
		histArea += hist[idx];

	int sumBins = 0;

	for (int idx = 255; idx >= 0; idx--) {

		sumBins += hist[idx];
		
		if (sumBins/(float)histArea > quantile) {
			qDebug() << "max bin: " << idx;
			return (uchar)idx;
		}

	}

	qDebug() << "no max bin found... sum: " << sumBins;

	return 255;
}

QPixmap DkImage::colorizePixmap(const QPixmap& icon, const QColor& col, float opacity) {

	if (icon.isNull())
		return icon;

	QPixmap glow = icon.copy();
	QPixmap sGlow = glow.copy();
	sGlow.fill(col);

	QPainter painter(&glow);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);
	painter.setCompositionMode(QPainter::CompositionMode_SourceIn);	// check if this is the right composition mode
	painter.setOpacity(opacity);
	painter.drawPixmap(glow.rect(), sGlow);

	return glow;
};

cv::Mat DkImage::get1DGauss(double sigma) {

	// correct -> checked with matlab reference
	int kernelsize = cvRound(cvCeil(sigma*3)*2)+1;
	if (kernelsize < 3) kernelsize = 3;
	if ((kernelsize % 2) != 1) kernelsize+=1;

	Mat gKernel = Mat(1, kernelsize, CV_32F);
	float* kernelPtr = gKernel.ptr<float>();

	for (int idx = 0, x = -cvFloor(kernelsize/2); idx < kernelsize; idx++,x++) {

		kernelPtr[idx] = (float)(exp(-(x*x)/(2*sigma*sigma)));	// 1/(sqrt(2pi)*sigma) -> discrete normalization
	}


	if (sum(gKernel).val[0] == 0)
		throw DkIllegalArgumentException("The kernel sum is zero\n", __LINE__, __FILE__);
	else
		gKernel *= 1.0f/sum(gKernel).val[0];

	return gKernel;
}

bool DkImage::unsharpMask(QImage& img, float sigma, float weight) {

#ifdef WITH_OPENCV
	DkTimer dt;
	//DkImage::gammaToLinear(img);
	cv::Mat imgCv = DkImage::qImage2Mat(img);

	cv::Mat imgG;
	cv::Mat gx = cv::getGaussianKernel(4*sigma+1, sigma);
	cv::Mat gy = gx.t();
	cv::sepFilter2D(imgCv, imgG, CV_8U, gx, gy);
	//cv::GaussianBlur(imgCv, imgG, cv::Size(4*sigma+1, 4*sigma+1), sigma);		// this is awesomely slow
	cv::addWeighted(imgCv, weight, imgG, 1-weight, 0, imgCv);
	img = DkImage::mat2QImage(imgCv);

	qDebug() << "unsharp mask takes: " << dt.getTotal();
	//DkImage::linearToGamma(img);
#endif


	return true;
}

QImage DkImage::createThumb(const QImage& image) {

	if (image.isNull())
		return image;

	int maxThumbSize = 160;
	int imgW = image.width();
	int imgH = image.height();

	if (imgW > maxThumbSize || imgH > maxThumbSize) {
		if (imgW > imgH) {
			imgH = (float)maxThumbSize / imgW * imgH;
			imgW = maxThumbSize;
		} 
		else if (imgW < imgH) {
			imgW = (float)maxThumbSize / imgH * imgW;
			imgH = maxThumbSize;
		}
		else {
			imgW = maxThumbSize;
			imgH = maxThumbSize;
		}
	}

	// fast downscaling
	QImage thumb = image.scaled(QSize(imgW*2, imgH*2), Qt::KeepAspectRatio, Qt::FastTransformation);
	thumb = thumb.scaled(QSize(imgW, imgH), Qt::KeepAspectRatio, Qt::SmoothTransformation);

	qDebug() << "thumb size in createThumb: " << thumb.size() << " format: " << thumb.format();

	return thumb;
};

QColor DkImage::getMeanColor(const QImage& img) {

	// some speed-up params
	int nC = qRound(img.depth()/8.0f);
	int rStep = qRound(img.height()/100.0f)+1;
	int cStep = qRound(img.width()/100.0f)+1;
	int numCols = 42;

	int offset = (nC > 1) ? 1 : 0;	// no offset for grayscale images
	QMap<QRgb, int> colLookup;
	int maxColCount = 0;
	QRgb maxCol;

	for (int rIdx = 0; rIdx < img.height(); rIdx += rStep) {

		const unsigned char* pixel = img.constScanLine(rIdx);

		for (int cIdx = 0; cIdx < img.width()*nC; cIdx += cStep*nC) {

			QColor cColC(qRound(pixel[cIdx+2*offset]/255.0f*numCols), 
				qRound(pixel[cIdx+offset]/255.0f*numCols), 
				qRound(pixel[cIdx]/255.0f*numCols));
			QRgb cCol = cColC.rgb();

			//// skip black
			//if (cColC.saturation() < 10)
			//	continue;
			if (qRed(cCol) < 3 && qGreen(cCol) < 3 && qBlue(cCol) < 3)
				continue;
			if (qRed(cCol) > numCols-3 && qGreen(cCol) > numCols-3 && qBlue(cCol) > numCols-3)
				continue;


			if (colLookup.contains(cCol)) {
				colLookup[cCol]++;
			}
			else
				colLookup[cCol] = 1;

			if (colLookup[cCol] > maxColCount) {
				maxCol = cCol;
				maxColCount = colLookup[cCol];
			}
		}
	}

	if (maxColCount > 0)
		return QColor((float)qRed(maxCol)/numCols*255, (float)qGreen(maxCol)/numCols*255, (float)qBlue(maxCol)/numCols*255);
	else
		return DkSettings::display.bgColorWidget;
}


// DkImageStorage --------------------------------------------------------------------
DkImageStorage::DkImageStorage(QImage img) {
	this->img = img;

	computeThread = new QThread;
	computeThread->start();
	moveToThread(computeThread);

	busy = false;
	stop = true;
}

void DkImageStorage::setImage(QImage img) {

	stop = true;
	imgs.clear();	// is it save (if the thread is still working?)
	this->img = img;
}

void DkImageStorage::antiAliasingChanged(bool antiAliasing) {

	DkSettings::display.antiAliasing = antiAliasing;

	if (!antiAliasing) {
		stop = true;
		imgs.clear();
	}

	emit imageUpdated();

}

QImage DkImageStorage::getImage(float factor) {

	if (factor >= 0.5f || img.isNull() || !DkSettings::display.antiAliasing)
		return img;

	// check if we have an image similar to that requested
	for (int idx = 0; idx < imgs.size(); idx++) {

		if ((float)imgs.at(idx).height()/img.height() >= factor)
			return imgs.at(idx);
	}

	// if the image does not exist - create it
	if (!busy && imgs.empty() && /*img.colorTable().isEmpty() &&*/ img.width() > 32 && img.height() > 32) {
		stop = false;
		// nobody is busy so start working
		QMetaObject::invokeMethod(this, "computeImage", Qt::QueuedConnection);
	}

	// currently no alternative is available
	return img;
}

void DkImageStorage::computeImage() {

	// obviously, computeImage gets called multiple times in some wired cases...
	if (!imgs.empty())
		return;

	DkTimer dt;
	busy = true;
	QImage resizedImg = img;
	

	// down sample the image until it is twice times full HD
	QSize iSize = img.size();
	while (iSize.width() > 2*1920 && iSize.height() > 2*1920)	// in general we need less than 200 ms for the whole downscaling if we start at 1500 x 1500
		iSize *= 0.5;

	// for extreme panorama images the Qt scaling crashes (if we have a width > 30000) so we simply 
	if (qMax(iSize.width(), iSize.height()) < 20000)
		resizedImg = resizedImg.scaled(iSize, Qt::KeepAspectRatio, Qt::FastTransformation);

	// it would be pretty strange if we needed more than 30 sub-images
	for (int idx = 0; idx < 30; idx++) {

		QSize s = resizedImg.size();
		s *= 0.5;

		if (s.width() < 32 || s.height() < 32)
			break;

		// // mapping here introduces bugs
		//DkImage::gammaToLinear(resizedImg);

#ifdef WITH_OPENCV
		cv::Mat rImgCv = DkImage::qImage2Mat(resizedImg);
		cv::Mat tmp;
		cv::resize(rImgCv, tmp, cv::Size(s.width(), s.height()), 0, 0, CV_INTER_AREA);
		resizedImg = DkImage::mat2QImage(tmp);
#else
		resizedImg = resizedImg.scaled(s, Qt::KeepAspectRatio, Qt::SmoothTransformation);
#endif

		// // mapping here introduces bugs
		//DkImage::linearToGamma(resizedImg);
		
		//resizedImg.setColorTable(img.colorTable());		// Not sure why we turned the color tables off

		// new image assigned?
		if (stop)
			break;

		mutex.lock();
		imgs.push_front(resizedImg);
		mutex.unlock();
	}

	busy = false;

	// tell my caller I did something
	emit imageUpdated();

	qDebug() << "pyramid computation took me: " << dt.getTotal() << " layers: " << imgs.size();

	if (imgs.size() > 6)
		qDebug() << "layer size > 6: " << img.size();

}

}
