/*******************************************************************************************************
 DkImageContainer.h
 Created on:	21.02.2014
 
 nomacs is a fast and small image viewer with the capability of synchronizing multiple instances
 
 Copyright (C) 2011-2014 Markus Diem <markus@nomacs.org>
 Copyright (C) 2011-2014 Stefan Fiel <stefan@nomacs.org>
 Copyright (C) 2011-2014 Florian Kleber <florian@nomacs.org>

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

#pragma once

#include <QObject>
#include <QImage>
#include <QFileInfo>
#include <QFuture>
#include <QFutureWatcher>
#include <QTimer>

#include "DkMetaData.h"
#include "DkBasicLoader.h"
#include "DkThumbs.h"


#ifndef DllExport
#ifdef DK_DLL_EXPORT
#define DllExport Q_DECL_EXPORT
#elif DK_DLL_IMPORT
#define DllExport Q_DECL_IMPORT
#else
#define DllExport
#endif
#endif

namespace nmc {

class DllExport DkImageContainer {

public:
	enum {
		loading_canceled = -3,
		loading = -2,
		exists_not = -1,
		not_loaded,
		loaded,
	};

	DkImageContainer(const QFileInfo& fileInfo);
	//friend bool operator==(const DkImageContainer& lic, const DkImageContainer& ric);
	bool operator==(const DkImageContainer& ric) const;
	bool operator< (const DkImageContainer& o) const;
	bool operator<= (const DkImageContainer& o) const;
	bool operator> (const DkImageContainer& o) const;
	bool operator>= (const DkImageContainer& o) const;

	QImage image();

	bool hasImage() const;
	int getLoadState() const;
	QFileInfo file() const;
	bool isEdited() const;
	bool isSelected() const;
	int getPageIdx() const;
	QString getTitleAttribute() const;
	QSharedPointer<DkBasicLoader> getLoader() const;
	QSharedPointer<DkMetaDataT> getMetaData() const;
	QSharedPointer<DkThumbNailT> getThumb() const;
	float getMemoryUsage() const;
	float getFileSize() const;

	bool exists();
	bool setPageIdx(int skipIdx);

	QSharedPointer<QByteArray> loadFileToBuffer(const QFileInfo fileInfo);
	bool loadImage();
	void setImage(const QImage& img, const QFileInfo& fileInfo);
	void saveMetaData();
	virtual void clear();

protected:
	QFileInfo fileInfo;
	QSharedPointer<QByteArray> fileBuffer;
	QSharedPointer<DkBasicLoader> loader;
	QSharedPointer<DkThumbNailT> thumb;

	int loadState;
	bool edited;
	bool selected;

	QSharedPointer<DkBasicLoader> loadImageIntern(const QFileInfo fileInfo, QSharedPointer<DkBasicLoader> loader, const QSharedPointer<QByteArray> fileBuffer);
	QFileInfo saveImageIntern(const QFileInfo fileInfo, QSharedPointer<DkBasicLoader> loader, QImage saveImg, int compression);
	void saveMetaDataIntern(const QFileInfo fileInfo, QSharedPointer<DkBasicLoader> loader, QSharedPointer<QByteArray> fileBuffer = QSharedPointer<QByteArray>());
	void init();
};

bool imageContainerLessThan(const DkImageContainer& l, const DkImageContainer& r);
bool imageContainerLessThanPtr(const QSharedPointer<DkImageContainer> l, const QSharedPointer<DkImageContainer> r);

class DllExport DkImageContainerT : public QObject, public DkImageContainer {
	Q_OBJECT

public:
	DkImageContainerT(const QFileInfo& file);
	virtual ~DkImageContainerT();

	void fetchFile();
	void cancel();
	void clear();
	void receiveUpdates(QObject* obj, bool connectSignals = true);

	bool loadImageThreaded(bool force = false);
	bool saveImageThreaded(const QFileInfo fileInfo, const QImage saveImg, int compression = -1);
	bool saveImageThreaded(const QFileInfo fileInfo, int compression = -1);
	void saveMetaDataThreaded();

signals:
	void fileLoadedSignal(bool loaded = true);
	void fileSavedSignal(QFileInfo fileInfo, bool saved = true);
	void showInfoSignal(QString msg, int time = 3000, int position = 0);
	void errorDialogSignal(const QString& msg);
	void thumbLoadedSignal(bool loaded = true);

public slots:
	void checkForFileUpdates(); 

protected slots:
	void bufferLoaded();
	void imageLoaded();
	void savingFinished();
	void loadingFinished();

protected:
	void fetchImage();
	
	QSharedPointer<QByteArray> loadFileToBuffer(const QFileInfo fileInfo);
	QSharedPointer<DkBasicLoader> loadImageIntern(const QFileInfo fileInfo, QSharedPointer<DkBasicLoader> loader, const QSharedPointer<QByteArray> fileBuffer);
	QFileInfo saveImageIntern(const QFileInfo fileInfo, QSharedPointer<DkBasicLoader> loader, QImage saveImg, int compression);
	void saveMetaDataIntern(QFileInfo fileInfo, QSharedPointer<DkBasicLoader> loader, QSharedPointer<QByteArray> fileBuffer);

	QFutureWatcher<QSharedPointer<QByteArray> > bufferWatcher;
	QFutureWatcher<QSharedPointer<DkBasicLoader> > imageWatcher;
	QFutureWatcher<QFileInfo> saveImageWatcher;
	QFutureWatcher<bool> saveMetaDataWatcher;

	bool fetchingImage;
	bool fetchingBuffer;
	bool waitForUpdate;

	QTimer fileUpdateTimer;
	//bool savingImage;
	//bool savingMetaData;
};

};