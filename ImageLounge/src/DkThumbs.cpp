/*******************************************************************************************************
 DkThumbs.cpp
 Created on:	19.04.2013
 
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

#include "DkThumbs.h"

#include "DkImage.h"

namespace nmc {


int DkThumbsLoader::maxThumbSize = 160;

/**
* Default constructor.
* @param file the corresponding file
* @param img the thumbnail image
**/ 
DkThumbNail::DkThumbNail(QFileInfo file, QImage img) {
	this->img = DkImage::createThumb(img);
	this->file = file;
	this->maxThumbSize = 160;
	this->minThumbSize = DkSettings::display.thumbSize;
	this->rescale = true;
	imgExists = true;
	meanColor = DkSettings::display.bgColorWidget;
	s = qMax(img.width(), img.height());
};

void DkThumbNail::compute(int forceLoad) {
	
	// we do this that complicated to be thread-safe
	// if we use member vars in the thread and the object gets deleted during thread execution we crash...
	this->img = computeIntern(file, QSharedPointer<QByteArray>(), forceLoad, maxThumbSize, minThumbSize, rescale);
}

QColor DkThumbNail::computeColorIntern() {

	QImage img = computeIntern(file, QSharedPointer<QByteArray>(), force_exif_thumb, maxThumbSize, minThumbSize, rescale);

	if (!img.isNull())
		return DkImage::getMeanColor(img);

	return DkSettings::display.bgColorWidget;
}

/**
 * Loads the thumbnail from the metadata.
 * If no thumbnail is embedded, the whole image
 * is loaded and downsampled in a fast manner.
 * @param file the file to be loaded
 * @return QImage the loaded image. Null if no image
 * could be loaded at all.
 **/ 
QImage DkThumbNail::computeIntern(const QFileInfo file, const QSharedPointer<QByteArray> ba, 
								  int forceLoad, int maxThumbSize, int minThumbSize, 
								  bool rescale) {
	
	DkTimer dt;
	//qDebug() << "[thumb] file: " << file.absoluteFilePath();

	// see if we can read the thumbnail from the exif data
	QImage thumb;
	DkMetaDataT metaData;

	try {
		if (!ba || ba->isEmpty())
			metaData.readMetaData(file);
		else
			metaData.readMetaData(file, ba);

		thumb = metaData.getThumbnail();
	}
	catch(...) {
		// do nothing - we'll load the full file
	}

	removeBlackBorder(thumb);

	if (thumb.isNull() && forceLoad == force_exif_thumb)
		return QImage();

	bool exifThumb = !thumb.isNull();

	int orientation = metaData.getOrientation();
	int imgW = thumb.width();
	int imgH = thumb.height();
	int tS = minThumbSize;

	// as found at: http://olliwang.com/2010/01/30/creating-thumbnail-images-in-qt/
	QString filePath = (file.isSymLink()) ? file.symLinkTarget() : file.absoluteFilePath();
	QImageReader* imageReader;
	
	if (!ba || ba->isEmpty())
		imageReader = new QImageReader(filePath);
	else {
		QBuffer buffer;
		buffer.setData(ba->data());
		buffer.open(QIODevice::ReadOnly);
		imageReader = new QImageReader(&buffer, QFileInfo(filePath).suffix().toStdString().c_str());
		buffer.close();
	}

	if (thumb.isNull() || thumb.width() < tS && thumb.height() < tS) {

		imgW = imageReader->size().width();
		imgH = imageReader->size().height();	// locks the file!
	}
	//else if (!thumb.isNull())
	//	qDebug() << "EXIV thumb loaded: " << thumb.width() << " x " << thumb.height();
	
	if (rescale && (imgW > maxThumbSize || imgH > maxThumbSize)) {
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

	if (thumb.isNull() || thumb.width() < tS && thumb.height() < tS || forceLoad == force_full_thumb || forceLoad == force_save_thumb) {
		
		// flip size if the image is rotated by 90�
		if (metaData.isTiff() && abs(orientation) == 90) {
			int tmpW = imgW;
			imgW = imgH;
			imgH = tmpW;
		}

		QSize initialSize = imageReader->size();

		imageReader->setScaledSize(QSize(imgW, imgH));
		thumb = imageReader->read();

		// try to read the image
		if (thumb.isNull()) {
			DkBasicLoader loader;
			
			if (loader.loadGeneral(file, ba, true, true))
				thumb = loader.image();
		}

		// the image is not scaled correctly yet
		if (rescale && !thumb.isNull() && (imgW == -1 || imgH == -1)) {
			imgW = thumb.width();
			imgH = thumb.height();

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

			thumb = thumb.scaled(QSize(imgW*2, imgH*2), Qt::KeepAspectRatio, Qt::FastTransformation);
			thumb = thumb.scaled(QSize(imgW, imgH), Qt::KeepAspectRatio, Qt::SmoothTransformation);
		}

		// is there a nice solution to do so??
		imageReader->setFileName("josef");	// image reader locks the file -> but there should not be one so we just set it to another file...
		delete imageReader;

	}
	else if (rescale) {
		thumb = thumb.scaled(QSize(imgW, imgH), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		//qDebug() << "thumb loaded from exif...";
	}

	if (orientation != -1 && orientation != 0 && (metaData.isJpg() || metaData.isRaw())) {
		QTransform rotationMatrix;
		rotationMatrix.rotate((double)orientation);
		thumb = thumb.transformed(rotationMatrix);
	}

	// save the thumbnail if the caller either forces it, or the save thumb is requested and the image did not have any before
	if (forceLoad == force_save_thumb || (forceLoad == save_thumb && !exifThumb)) {
		
		try {

			QImage sThumb = thumb.copy();
			if (orientation != -1 && orientation != 0) {
				QTransform rotationMatrix;
				rotationMatrix.rotate(-(double)orientation);
				sThumb = sThumb.transformed(rotationMatrix);
			}

			metaData.setThumbnail(thumb);

			if (!ba || ba->isEmpty())
				metaData.saveMetaData(file);
			else
				metaData.saveMetaData(file, ba);
		}
		catch(...) {
			qDebug() << "Sorry, I could not save the metadata";
		}
	}


	if (!thumb.isNull())
		qDebug() << "[thumb] " << file.fileName() << " loaded in: " << dt.getTotal() << ((exifThumb) ? " from EXIV" : " from File");

	//if (!thumb.isNull())
	//	qDebug() << "thumb: " << thumb.width() << " x " << thumb.height();


	return thumb;
}

void DkThumbNail::removeBlackBorder(QImage& img) {

	int rIdx = 0;
	bool nonblack = false;
	
	for ( ; rIdx < qRound(img.height()*0.1); rIdx++) {

		const QRgb* pixel = (QRgb*)(img.constScanLine(rIdx));

		for (int cIdx = 0; cIdx < img.width(); cIdx++, pixel++) {

			// > 50 due to jpeg (normally we would want it to be != 0)
			if (qRed(*pixel) > 50 || qBlue(*pixel) > 50 || qGreen(*pixel) > 50) {
				nonblack = true;
				break;
			}
		}

		if (nonblack)
			break;
	}

	// non black border?
	if (rIdx == -1 || rIdx > 15)
		return;

	int rIdxB = img.height()-1;
	nonblack = false;

	for ( ; rIdxB >= qRound(img.height()*0.9f); rIdxB--) {

		const QRgb* pixel = (QRgb*)(img.constScanLine(rIdxB));

		for (int cIdx = 0; cIdx < img.width(); cIdx++, pixel++) {

			if (qRed(*pixel) > 50 || qBlue(*pixel) > 50 || qGreen(*pixel) > 50) {
				nonblack = true;
				break;
			}
		}

		if (nonblack) {
			rIdxB--;
			break;
		}
	}

	// remove black borders
	if (rIdx < rIdxB)
		img = img.copy(0, rIdx, img.width(), rIdxB-rIdx);

}

void DkThumbNail::setImage(QImage img) {
	
	this->img = DkImage::createThumb(img);
}

DkThumbNailT::DkThumbNailT(QFileInfo file, QImage img) : DkThumbNail(file, img) {

	fetching = false;
	fetchingColor = false;
	forceLoad = do_not_force;

	connect(&thumbWatcher, SIGNAL(finished()), this, SLOT(thumbLoaded()));
	connect(&colorWatcher, SIGNAL(finished()), this, SLOT(colorLoaded()));
}

DkThumbNailT::~DkThumbNailT() {

	if (fetching && DkSettings::resources.numThumbsLoading > 0)
		DkSettings::resources.numThumbsLoading--;
	thumbWatcher.blockSignals(true);
	thumbWatcher.cancel();
}

void DkThumbNailT::fetchColor() {
	
	if (meanColor != DkSettings::display.bgColorWidget || !imgExists || fetchingColor)
		return;

	// we have to do our own bool here
	// watcher.isRunning() returns false if the thread is waiting in the pool
	fetchingColor = true;

	QFuture<QColor> future = QtConcurrent::run(this, 
		&nmc::DkThumbNailT::computeColorCall);

	colorWatcher.setFuture(future);
}

QColor DkThumbNailT::computeColorCall() {

	return DkThumbNail::computeColorIntern();
}

void DkThumbNailT::colorLoaded() {

	QFuture<QColor> future = colorWatcher.future();

	meanColor = future.result();

	if (meanColor != DkSettings::display.bgColorWidget)
		emit colorUpdated();
	else
		colorExists = false;

	fetchingColor = false;
	qDebug() << "mean color: " << meanColor;
}

bool DkThumbNailT::fetchThumb(int forceLoad /* = false */,  QSharedPointer<QByteArray> ba) {

	if (forceLoad == force_full_thumb || forceLoad == force_save_thumb || forceLoad == save_thumb)
		img = QImage();

	if (!img.isNull() || !imgExists || fetching)
		return false;

	// we have to do our own bool here
	// watcher.isRunning() returns false if the thread is waiting in the pool
	fetching = true;
	this->forceLoad = forceLoad;

	QFuture<QImage> future = QtConcurrent::run(this, 
		&nmc::DkThumbNailT::computeCall, forceLoad, ba);

	thumbWatcher.setFuture(future);
	DkSettings::resources.numThumbsLoading++;

	return true;
}


QImage DkThumbNailT::computeCall(int forceLoad, QSharedPointer<QByteArray> ba) {

	return DkThumbNail::computeIntern(file, ba, forceLoad, maxThumbSize, minThumbSize, rescale);
}

void DkThumbNailT::thumbLoaded() {
	
	QFuture<QImage> future = thumbWatcher.future();

	img = future.result();
	
	if (img.isNull() && forceLoad != force_exif_thumb)
		imgExists = false;

	fetching = false;
	DkSettings::resources.numThumbsLoading--;
	emit thumbLoadedSignal(img.isNull());
}

//// DkThumbPool --------------------------------------------------------------------
//DkThumbPool::DkThumbPool(QFileInfo file /* = QFileInfo */, QObject* parent /* = 0 */) : QObject(parent) {
//	this->currentFile = file;
//}
//
//void DkThumbPool::setFile(const QFileInfo& file, int force) {
//
//	// >DIR: TODO: updating is not working properly e.g. DSC_4068.jpg  [19.12.2013 markus]
//	qDebug() << "[thumbpool] current file: " << currentFile.absoluteFilePath() << " new file: " << file.absoluteFilePath();
//
//	if (!file.exists()) {
//		qDebug() << file.absoluteFilePath() << " does not exist";
//		return;
//	}
//
//	if (!listenerList.empty() && (force == DkThumbsLoader::user_updated || dir(currentFile) != dir(file)))
//		indexDir(file);
//	else if (!listenerList.empty() && force == DkThumbsLoader::dir_updated)
//		updateDir(file);
//
//	if (currentFile != file || force != DkThumbsLoader::not_forced)
//		emit newFileIdxSignal(fileIdx(file));
//
//	currentFile = file;
//}
//
//QFileInfo DkThumbPool::getCurrentFile() {
//	return currentFile;
//}
//
//QDir DkThumbPool::dir(const QFileInfo& file) const {
//
//	return (file.isDir()) ? QDir(file.absoluteFilePath()) : file.absoluteDir();
//}
//
//int DkThumbPool::fileIdx(const QFileInfo& file) {
//
//	int tIdx = -1;
//	
//	for (int idx = 0; idx < thumbs.size(); idx++) {
//		if (file == thumbs.at(idx)->getFile()) {
//			tIdx = idx;
//			break;
//		}
//	}
//
//	return tIdx;
//}
//
//int DkThumbPool::getCurrentFileIdx() {
//
//	if (thumbs.empty())
//		indexDir(currentFile);
//	
//	return fileIdx(currentFile);
//}
//
//QVector<QSharedPointer<DkThumbNailT> > DkThumbPool::getThumbs() {
//
//	if (thumbs.empty())
//		indexDir(currentFile);
//	
//	emit newFileIdxSignal(getCurrentFileIdx());
//
//	return thumbs;
//}
//
//void DkThumbPool::getUpdates(QObject* obj, bool isActive) {
//
//	bool registered = false;
//	for (int idx = 0; idx < listenerList.size(); idx++) {
//
//		if (!isActive && listenerList.at(idx) == obj) {
//			listenerList.remove(idx);
//			break;
//		}
//		else if (isActive && listenerList.at(idx) == obj) {
//			registered = true;
//			break;
//		}
//	}
//
//	if (!registered && isActive) {
//		
//		// we need an update here if the listener list was empty
//		if (listenerList.isEmpty())
//			updateDir(currentFile);
//
//		listenerList.append(obj);
//	}
//
//}
//
//void DkThumbPool::indexDir(const QFileInfo& currentFile) {
//
//	thumbs.clear();
//
//	// imho this is a Qt bug
//	QDir cDir = dir(currentFile);
//
//	files = DkImageLoader::getFilteredFileList(cDir);
//
//	for (int idx = 0; idx < files.size(); idx++) {
//		QSharedPointer<DkThumbNailT> t = createThumb(QFileInfo(cDir, files.at(idx)));
//		thumbs.append(t);
//	}
//	
//	if (!thumbs.empty())
//		emit numThumbChangedSignal();
//
//}
//
//void DkThumbPool::updateDir(const QFileInfo& currentFile) {
//
//	QVector<QSharedPointer<DkThumbNailT> > newThumbs;
//
//	QDir cDir = dir(currentFile);
//	files = DkImageLoader::getFilteredFileList(cDir);
//
//	for (int idx = 0; idx < files.size(); idx++) {
//
//		QFileInfo cFile(cDir, files.at(idx));
//		int fIdx = fileIdx(cFile);
//
//		if (fIdx != -1 && thumbs.at(fIdx)->getFile().lastModified() == cFile.lastModified())
//			newThumbs.append(thumbs.at(fIdx));
//		else {
//			QSharedPointer<DkThumbNailT> t = createThumb(cFile);
//			newThumbs.append(t);
//		}
//	}
//	
//	if (!thumbs.empty() && thumbs.size() != newThumbs.size())
//		emit numThumbChangedSignal();
//
//	thumbs = newThumbs;
//}
//
//QSharedPointer<DkThumbNailT> DkThumbPool::createThumb(const QFileInfo& file) {
//
//	QSharedPointer<DkThumbNailT> thumb(new DkThumbNailT(file));
//	connect(thumb.data(), SIGNAL(thumbUpdated()), this, SLOT(thumbUpdated()));
//	return thumb;
//}
//
//void DkThumbPool::thumbUpdated() {
//
//	// maybe we have to add a timer here to ignore too many calls at the same time
//	emit thumbUpdatedSignal();
//}


/**
 * Default constructor of the thumbnail loader.
 * Note: currently the init calls the getFilteredFileList which might be slow.
 * @param thumbs a pointer to an array holding the thumbnails. while
 * loading, the thumbsloader will add all images to this array. however, the
 * caller must destroy the thumbs vector.
 * @param dir the directory where thumbnails should be loaded from.
 **/ 
DkThumbsLoader::DkThumbsLoader(std::vector<DkThumbNail>* thumbs, QDir dir, QFileInfoList files) {

	this->thumbs = thumbs;
	this->dir = dir;
	this->isActive = true;
	this->files = files;
	init();
}

/**
 * Initializes the thumbs loader.
 * Note: getFilteredFileList might be slow.
 **/ 
void DkThumbsLoader::init() {

	// TODO: update!
	//if (files.empty())
	//	files = DkImageLoader::getFilteredFileInfoList(dir);
	startIdx = -1;
	endIdx = -1;
	somethingTodo = false;
	numFilesLoaded = 0;
	loadAllThumbs = false;
	forceSave = false;
	forceLoad = false;

	// here comes hot stuff (for a better update policy)
	std::vector<DkThumbNail> oldThumbs = *thumbs;
	thumbs->clear();

	DkTimer dt;
	for (int idx = 0; idx < files.size(); idx++) {
		QFileInfo cFile = files[idx];

		DkThumbNail cThumb = DkThumbNail(cFile);

		for (unsigned int idx = 0; idx < oldThumbs.size(); idx++) {

			if (cThumb == oldThumbs[idx]) {
				cThumb = oldThumbs[idx];
				break;
			}
		}

		thumbs->push_back(cThumb);
	}

	qDebug() << "thumb stubs loaded in: " << dt.getTotal();
}

/**
 * Returns the file idx of the file specified.
 * @param file the file to be queried.
 * @return int the index of the file.
 **/ 
int DkThumbsLoader::getFileIdx(QFileInfo& file) {

	//mutex.lock();

	if (!file.exists() || !thumbs)
		return -1;

	QString cFilePath = file.absoluteFilePath();
	unsigned int fileIdx = 0;
	for ( ; fileIdx < thumbs->size(); fileIdx++) {

		if (thumbs->at(fileIdx).getFile().absoluteFilePath() == cFilePath)
			break;
	}

	if (fileIdx == thumbs->size()) fileIdx = -1;

	//mutex.unlock();

	return fileIdx;

}

/**
 * Thread routine.
 * Only loads thumbs if somethingTodo is true.
 **/ 
void DkThumbsLoader::run() {

	if (!thumbs)
		return;

	while (true) {

		if (loadAllThumbs && numFilesLoaded >= (int)thumbs->size()) {
			qDebug() << "[thumbs] thinks he has finished...";
			break;
		}

		mutex.lock();
		DkTimer dt;
		msleep(100);

		//QMutexLocker(&this->mutex);
		if (!isActive) {
			qDebug() << "thumbs loader stopped...";
			mutex.unlock();
			break;
		}
		mutex.unlock();

		if (somethingTodo)
			loadThumbs();
	}

	//// locate the current file
	//QStringList files = dir.entryList(DkImageLoader::fileFilters);

	//DkTimer dtt;

	//for (int idx = 0; idx < files.size(); idx++) {

	//	QMutexLocker(&this->mutex);
	//	if (!isActive) {
	//		break;
	//	}

	//	QFileInfo cFile = QFileInfo(dir, files[idx]);

	//	if (!cFile.exists() || !cFile.isReadable())
	//		continue;

	//	QImage img = getThumbNailQt(cFile);
	//	//QImage img = getThumbNailWin(cFile);
	//	thumbs->push_back(DkThumbNail(cFile, img));
	//}

}

/**
 * Loads thumbnails from the metadata.
 **/ 
void DkThumbsLoader::loadThumbs() {

	std::vector<DkThumbNail>::iterator thumbIter = thumbs->begin()+startIdx;
	qDebug() << "start: " << startIdx << " end: " << endIdx;

	for (int idx = startIdx; idx < endIdx; idx++, thumbIter++) {

		mutex.lock();

		// jump to new start idx
		if (startIdx > idx) {
			thumbIter = thumbs->begin()+startIdx;
			idx = startIdx;
		}

		// does somebody want me to stop?
		if (!isActive) {
			mutex.unlock();
			return;
		}
		
		// TODO:  he breaks here! (crash detected++)
		// at the same time, main thread in DkFilePreview indexDir() -> waiting for our loader after stopping it
		DkThumbNail* thumb = &(*thumbIter);
		if (!thumb->hasImage()) {
			thumb->compute(forceLoad);
			if (thumb->hasImage())	// could I load the thumb?
				emit updateSignal();
			else {
				thumb->setImgExists(false);
				qDebug() << "image does NOT exist...";
			}
			
		}
		emit numFilesSignal(++numFilesLoaded);
		mutex.unlock();
	}

	somethingTodo = false;
}

/**
 * Here you can specify which thumbnails to load.
 * Note: it is not a good idea to load all thumbnails
 * of a folder (might be a lot : )
 * @param start the start index
 * @param end the end index
 **/ 
void DkThumbsLoader::setLoadLimits(int start, int end) {

	//QMutexLocker(&this->mutex);
	//if (start < startIdx || startIdx == -1)	startIdx = (start >= 0 && start < thumbs->size()) ? start : 0;
	//if (end > endIdx || endIdx == -1)		endIdx = (end > 0 && end < thumbs->size()) ? end : thumbs->size();
	startIdx = (start >= 0 && (unsigned int) start < thumbs->size()) ? start : 0;
	endIdx = (end > 0 && (unsigned int) end < thumbs->size()) ? end : (int)thumbs->size();



	//somethingTodo = true;
}

/**
 * This function is used for batch saving.
 * If this function is called, all thumbs are saved 
 * even if save is not checked in the preferences.
 **/ 
void DkThumbsLoader::loadAll() {

	if (!thumbs)
		return;

	// this function is used for batch saving
	loadAllThumbs = true;
	forceSave = true;
	somethingTodo = true;
	setLoadLimits(0, (int)thumbs->size());
}

//QImage DkThumbsLoader::getThumbNailWin(QFileInfo file) {
//
//	CoInitialize(NULL);
//
//	DkTimer dt;
//
//	QImage thumb;
//
//	// allocate some unmanaged memory for our strings and divide the file name
//	// into a folder path and file name.
//	//String* fileName = file.absoluteFilePath();
//	//IntPtr dirPtr = Marshal::StringToHGlobalUni(Path::GetDirectoryName(fileName));
//	//IntPtr filePtr = Marshal::StringToHGlobalUni(Path::GetFileName(fileName));
//
//	QString winPath = QDir::toNativeSeparators(file.absolutePath());
//	QString winFile = QDir::toNativeSeparators(file.fileName());
//	winPath.append("\\");	
//
//	WCHAR* wDirName = new WCHAR[winPath.length()];
//	WCHAR* wFileName = new WCHAR[winFile.length()];
//
//	int dirLength = winPath.toWCharArray(wDirName);
//	int fileLength = winFile.toWCharArray(wFileName);
//
//	wDirName[dirLength] = L'\0';
//	wFileName[fileLength] = L'\0';
//
//	IShellFolder* pDesktop = NULL;
//	IShellFolder* pSub = NULL;
//	IExtractImage* pIExtract = NULL;
//	LPITEMIDLIST pList = NULL;
//
//	// get the desktop directory
//	if (SUCCEEDED(SHGetDesktopFolder(&pDesktop)))
//	{   
//		// get the pidl for the directory
//		HRESULT hr = pDesktop->ParseDisplayName(NULL, NULL, wDirName, NULL, &pList, NULL);
//		if (FAILED(hr)) {
//			//throw new Exception(S"Failed to parse the directory name");
//
//			return thumb;
//		}
//
//		// get the directory IShellFolder interface
//		hr = pDesktop->BindToObject(pList, NULL, IID_IShellFolder, (void**)&pSub);
//		if (FAILED(hr))	{
//			//throw new Exception(S"Failed to bind to the directory");
//			return thumb;
//		}
//
//		// get the file's pidl
//		hr = pSub->ParseDisplayName(NULL, NULL, wFileName, NULL, &pList, NULL);
//		if (FAILED(hr))	{
//			//throw new Exception(S"Failed to parse the file name");
//			return thumb;
//		}
//
//		// get the IExtractImage interface
//		LPCITEMIDLIST pidl = pList;
//		hr = pSub->GetUIObjectOf(NULL, 1, &pidl, IID_IExtractImage,
//			NULL, (void**)&pIExtract);
//
//		// set our desired image size
//		SIZE size;
//		size.cx = maxThumbSize;
//		size.cy = maxThumbSize;      
//
//		if(pIExtract == NULL) {
//			return thumb;
//		}         
//
//		HBITMAP hBmp = NULL;
//
//		// The IEIFLAG_ORIGSIZE flag tells it to use the original aspect
//		// ratio for the image size. The IEIFLAG_QUALITY flag tells the 
//		// interface we want the image to be the best possible quality.
//		DWORD dwFlags = IEIFLAG_ORIGSIZE | IEIFLAG_QUALITY;      
//
//		OLECHAR pathBuffer[MAX_PATH];
//		hr = pIExtract->GetLocation(pathBuffer, MAX_PATH, NULL, &size, 4, &dwFlags);         // TODO: color depth!! (1)
//		if (FAILED(hr)) {
//			//throw new Exception(S"The call to GetLocation failed");
//			return thumb;
//		}
//
//		hr = pIExtract->Extract(&hBmp);
//
//		// It is possible for Extract to fail if there is no thumbnail image
//		// so we won't check for success here
//
//		pIExtract->Release();
//
//		if (hBmp != NULL) {
//			thumb = QPixmap::fromWinHBITMAP(hBmp, QPixmap::Alpha).toImage();
//		}      
//	}
//
//	// Release the COM objects we have a reference to.
//	pDesktop->Release();
//	pSub->Release(); 
//
//	// delete the unmanaged memory we allocated
//	//Marshal::FreeCoTaskMem(dirPtr);
//	//Marshal::FreeCoTaskMem(filePtr);
//	//delete[] wDirName;
//	//delete[] wFileName;
//
//
//	return thumb;
//}

/**
 * Stops the current loading process.
 * This method allows for stopping the thread without killing it.
 **/ 
void DkThumbsLoader::stop() {
	
	//QMutexLocker(&this->mutex);
	isActive = false;
	qDebug() << "stopping thread: " << this->thread()->currentThreadId();
}

}
