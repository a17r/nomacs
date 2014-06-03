#ifdef NIKON_API

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <QDebug>
#include <QFutureWatcher>
#include <QtConcurrentRun>
#include "MaidFacade.h"
#include "MaidUtil.h"
#include "MaidError.h"
#include "DkNoMacs.h"

using nmc::MaidFacade;
using Maid::MaidUtil;
using Maid::MaidObject;

MaidFacade::MaidFacade(nmc::DkNoMacs* noMacs) 
	: noMacs(noMacs), lensAttached(false), prevFileNumber(0), captureCount(0), allItemsAcquired(false), currentlyAcquiringObjects(false) {
}

/*!
 * throws InitError, MaidError
 */
void MaidFacade::init() {
	
	try {
		MaidUtil::getInstance().loadLibrary();
		qDebug() << "Loaded MAID library";
		MaidUtil::getInstance().initMAID();
		qDebug() << "Initialized MAID";
	} catch(...) {
		qDebug() << "Could not initialize MAID (whatever that is)";
		return;
	}

	// create module object
	moduleObject.reset(MaidObject::create(0, nullptr));
	moduleObject->enumCaps();
	qDebug() << "MAID Module Object created";
	// set callbacks
	moduleObject->setEventCallback(this, eventProc);
	//moduleObject->setProgressCallback(progressProc);
	//moduleObject->setUIRequestCallback(uiRequestProc);
	// set ModuleMode
	//if (moduleObject->hasCapOperation(kNkMAIDCapability_ModuleMode, kNkMAIDCapOperation_Set)) {
	//	moduleObject->capSet(kNkMAIDCapability_ModuleMode, kNkMAIDDataType_Unsigned, (NKPARAM) kNkMAIDModuleMode_Controller);
	//} else {
	//	qDebug() << "kNkMAIDCapability_ModuleMode can not be set";
	//}

	// connect future watchers
	connect(&shootFutureWatcher, SIGNAL(finished()), this, SLOT(shootFinished()));
	connect(&acquireFutureWatcher, SIGNAL(finished()), this, SLOT(acquireItemObjectsFinished()));
}

void MaidFacade::setCapValueChangeCallback(std::function<void(uint32_t)> capValueChangeCallback) {
	this->capValueChangeCallback = capValueChangeCallback;
}

/*!
 * throws MaidError
 */
std::set<uint32_t> MaidFacade::listDevices() {
	auto& devicesV = moduleObject->getChildren();
	return std::set<uint32_t>(devicesV.begin(), devicesV.end());
}

/*!
 * throws OpenCloseObjectError
 */
void MaidFacade::openSource(ULONG id) {
	sourceObject.reset(MaidObject::create(id, moduleObject.get()));
	sourceObject->setEventCallback(this, eventProc);
	//sourceObject->setProgressCallback(progressProc);
}

bool MaidFacade::checkCameraType() {
	// the only currently supported source is a Nikon D4
	// read the camera type
	ULONG cameraType = 0;
	sourceObject->capGet(kNkMAIDCapability_CameraType, kNkMAIDDataType_UnsignedPtr, (NKPARAM) &cameraType);
	return cameraType == kNkMAIDCameraType_D4;
}

/*!
 * Reads a packed string value from the source and returns it
 * throws MaidError
 */
MaidFacade::MaybeStringValues MaidFacade::readPackedStringCap(ULONG capId) {
	MaybeStringValues mv;
	StringValues& v = mv.first;
	mv.second = false;
	
	if (!sourceObject) {
		return mv;
	}
	
	std::pair<NkMAIDEnum, bool> mEnum = MaidUtil::getInstance().fillEnum<char>(sourceObject.get(), capId);
	if (!mEnum.second) {
		return mv;
	}

	NkMAIDEnum* en = &mEnum.first;
	v.values = MaidUtil::getInstance().packedStringEnumToVector(en);
	v.currentValue = en->ulValue;
	delete[] en->pData;

	mv.second = true;
	return mv;
}

MaidFacade::MaybeUnsignedValues MaidFacade::readUnsignedEnumCap(ULONG capId) {
	MaybeUnsignedValues mv;
	UnsignedValues& v = mv.first;
	mv.second = false;

	if (!sourceObject) {
		return mv;
	}

	std::pair<NkMAIDEnum, bool> mEnum = MaidUtil::getInstance().fillEnum<ULONG>(sourceObject.get(), capId);
	if (!mEnum.second) {
		return mv;
	}
	
	NkMAIDEnum* en = &mEnum.first;
	v.values = MaidUtil::getInstance().unsignedEnumToVector(en);
	v.currentValue = en->ulValue;
	delete[] en->pData;

	mv.second = true;
	return mv;
}

/*!
 * Reads the aperture value from the source and returns it
 * throws MaidError
 */
MaidFacade::MaybeStringValues MaidFacade::readAperture() {
	MaybeStringValues v = readPackedStringCap(kNkMAIDCapability_Aperture);
	// maid module returns "--" if there is no lens attached and F0Manual is not set
	if (!v.second || v.first.values.size() == 0 || v.first.values.at(0) == "--") {
		v.second = false;
		return v;
	}

	aperture = v;

	return aperture;
}

/*!
 * Reads the sensitivity value from the source and returns it
 * throws MaidError
 */
MaidFacade::MaybeStringValues MaidFacade::readSensitivity() {
	sensitivity = readPackedStringCap(kNkMAIDCapability_Sensitivity);
	return sensitivity;
}

/*!
 * Reads the shutter speed value from the source and returns it
 * throws MaidError
 */
MaidFacade::MaybeStringValues MaidFacade::readShutterSpeed() {
	shutterSpeed = readPackedStringCap(kNkMAIDCapability_ShutterSpeed);
	return shutterSpeed;
}

/*!
 * Reads the exposure mode from the source and returns it
 * throws MaidError
 */
MaidFacade::MaybeUnsignedValues MaidFacade::readExposureMode() {
	exposureMode = readUnsignedEnumCap(kNkMAIDCapability_ExposureMode);
	if (!exposureMode.second) {
		return exposureMode;
	}

	// lens attached: 4 values
	// not attached: 2 values
	lensAttached = exposureMode.first.values.size() == 4;

	return exposureMode;
}

MaidFacade::MaybeStringValues MaidFacade::getAperture() {
	return aperture;
}

MaidFacade::MaybeStringValues MaidFacade::getSensitivity() {
	return sensitivity;
}

MaidFacade::MaybeStringValues MaidFacade::getShutterSpeed() {
	return shutterSpeed;
}

MaidFacade::MaybeUnsignedValues MaidFacade::getExposureMode() {
	return exposureMode;
}

bool MaidFacade::writeEnumCap(ULONG capId, size_t newValue) {
	try {
		if (!sourceObject->hasCapOperation(capId, kNkMAIDCapOperation_Set)) {
			return false;
		}

		std::pair<NkMAIDEnum, bool> mEnum = MaidUtil::getInstance().fillEnum<char>(sourceObject.get(), capId);
		if (!mEnum.second) {
			return false;
		}

		mEnum.first.ulValue = newValue;
		sourceObject->capSet(capId, kNkMAIDDataType_EnumPtr, (NKPARAM) &mEnum.first);
		return true;
	} catch (Maid::MaidError) {
		return false; // we don't care about what specifically went wrong
	}
}

bool MaidFacade::setMaybeStringEnumValue(std::pair<StringValues, bool>& theMaybeValue, ULONG capId, size_t newValue) {
	if (theMaybeValue.second) {
		bool r = writeEnumCap(capId, newValue);
		if (r) {
			theMaybeValue.first.currentValue = newValue;
		}
		return r;
	} else {
		return false;
	}
}

bool MaidFacade::setMaybeUnsignedEnumValue(std::pair<UnsignedValues, bool>& theMaybeValue, ULONG capId, size_t newValue) {
	if (theMaybeValue.second) {
		bool r = writeEnumCap(capId, newValue);
		if (r) {
			theMaybeValue.first.currentValue = newValue;
		}
		return r;
	} else {
		return false;
	}
}

bool MaidFacade::setAperture(size_t newValue) {
	return setMaybeStringEnumValue(aperture, kNkMAIDCapability_Aperture, newValue);
}

bool MaidFacade::setSensitivity(size_t newValue) {
	return setMaybeStringEnumValue(sensitivity, kNkMAIDCapability_Sensitivity, newValue);
}

bool MaidFacade::setShutterSpeed(size_t newValue) {
	return setMaybeStringEnumValue(shutterSpeed, kNkMAIDCapability_ShutterSpeed, newValue);
}

bool MaidFacade::setExposureMode(size_t newValue) {
	return setMaybeUnsignedEnumValue(exposureMode, kNkMAIDCapability_ExposureMode, newValue);
}

bool MaidFacade::isLensAttached() {
	return lensAttached;
}

bool MaidFacade::isAutoIso() {
	bool autoIso;
	sourceObject->capGet(kNkMAIDCapability_IsoControl, kNkMAIDDataType_BooleanPtr, (NKPARAM) &autoIso);
	return autoIso;
}

void MaidFacade::closeModule() {
	if (moduleObject) {
		moduleObject->closeObject();
	}
	moduleObject.reset();
}
	
void MaidFacade::closeSource() {
	if (sourceObject) {
		try {
			if (isLiveViewActive()) {
				toggleLiveView();
			}

			sourceObject->closeObject();
		} catch (Maid::MaidError) {
			// live view not active or object was already closed
		}
	}
	sourceObject.reset();
}

void MaidFacade::closeEverything() {
	closeSource();
	closeModule();
}

bool MaidFacade::isSourceAlive() {
	if (sourceObject) {
		return sourceObject->isAlive();
	}
	return false;
}

void MaidFacade::sourceIdleLoop(ULONG* count) {
	// wait until the operation is completed (when completionProc is called)
	do {
		sourceObject->async();
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	} while (*count <= 0);
	sourceObject->async();
}

bool MaidFacade::shoot(bool withAf) {
	captureCount = 0;
	NkMAIDCapInfo capInfo;
	sourceObject->getCapInfo(kNkMAIDCapability_Capture, &capInfo);

	CompletionProcData* complData = new CompletionProcData();
	complData->count = &captureCount;

	unsigned long cap = kNkMAIDCapability_Capture;
	if (withAf) {
		cap = kNkMAIDCapability_AFCapture;
	}
	//int opRet = sourceObject->capStart(cap, completionProc, (NKREF) complData);

	if (!shootFutureWatcher.isRunning()) {
		// start shooting (threaded)
		QFuture<int> shootFuture = QtConcurrent::run(sourceObject.get(), &MaidObject::capStart, cap, (LPNKFUNC) completionProc, (NKREF) complData);
		shootFutureWatcher.setFuture(shootFuture);

		return true;
	}

	return false;
}

void MaidFacade::shootFinished() {
	int opRet = shootFutureWatcher.result();
	if (opRet != kNkMAIDResult_NoError && opRet != kNkMAIDResult_Pending) {
		qDebug() << "Error executing capture capability";
		return;// return false;
	}

	// start acquiring the pictures (threaded)
	sourceIdleLoop(&captureCount);
}

bool MaidFacade::acquireItemObjects() {
	CompletionProcData* complData;
	NkMAIDCapInfo capInfo;

	allItemsAcquired = false;

	// acquire the _next_ item object

	std::vector<ULONG> itemIds = sourceObject->getChildren();
	if (itemIds.size() <= 0) {
		qDebug() << "No item objects left";

		emit shootAndAcquireFinished();
		allItemsAcquired = true;

		return true;
	}

	// open the item object
	std::unique_ptr<MaidObject> itemObject(MaidObject::create(itemIds.at(0), sourceObject.get())); // we always choose the one at pos 0
	if (!itemObject) {
		qDebug() << "Item object #0 could not be opened!";
		return false;
	}

	itemObject->getCapInfo(kNkMAIDCapability_DataTypes, &capInfo);
	ULONG dataTypes;
	itemObject->capGet(kNkMAIDCapability_DataTypes, kNkMAIDDataType_UnsignedPtr, (NKPARAM) &dataTypes);

	std::unique_ptr<MaidObject> dataObject;

	if (dataTypes & kNkMAIDDataObjType_Image) {
		dataObject.reset(MaidObject::create(kNkMAIDDataObjType_Image, itemObject.get()));
	} else if (dataTypes & kNkMAIDDataObjType_File) {
		dataObject.reset(MaidObject::create(kNkMAIDDataObjType_File, itemObject.get()));
	} else {
		return false;
	}

	if (!dataObject) {
		qDebug() << "Data object could not be opened!";
		return false;
	}

	DataProcData* dataRef = new DataProcData(this);
	dataRef->id = dataObject->getID();

	ProgressProcData* progressRef = new ProgressProcData(this);

	ULONG acquireCount = 0;
	complData = new CompletionProcData();
	complData->count = &acquireCount;
	complData->data = dataRef;

	dataObject->setDataCallback((NKREF) dataRef, dataProc);
	dataObject->setProgressCallback((NKREF) progressRef, progressProc);
	int opRet = dataObject->capStart(kNkMAIDCapability_Acquire, completionProc, (NKREF) complData);
	if (opRet != kNkMAIDResult_NoError && opRet != kNkMAIDResult_Pending) {
		qDebug() << "Error acquiring data";
		return false;
	}

	sourceIdleLoop(&acquireCount);

	dataObject->setDataCallback((NKREF) nullptr, (LPMAIDDataProc) nullptr);

	return true;
}

void MaidFacade::startAcquireItemObjects() {
	if (!currentlyAcquiringObjects) {
		currentlyAcquiringObjects = true;
		emit acquireStart();
		QFuture<bool> acquireFuture = QtConcurrent::run(this, &MaidFacade::acquireItemObjects);
		acquireFutureWatcher.setFuture(acquireFuture);
	}
}

void MaidFacade::acquireItemObjectsFinished() {
	if (allItemsAcquired) {
		allItemsAcquired = false;
		currentlyAcquiringObjects = false;
		return;
	}

	[&] () {
		QString filename;
		QFileInfo tempFilenameInfo = QFileInfo(QString::fromStdString(makePictureFilename()));
		QFileInfo firstFilenameInfo = QFileInfo(firstFilename);

		// if it is the first picture or the file type has changed
		if (firstFilename.isEmpty()) {
			filename = noMacs->getCapturedFileName(tempFilenameInfo);
			if (filename.isEmpty()) {
				//return kNkMAIDResult_NoError;
				qDebug() << "filename was empty, file will be discarded";
				return;
			}
			firstFilename = filename;
		} else {
			// save 
			QFileInfo newFilenameInfo = QFileInfo(firstFilenameInfo.baseName() + "." + tempFilenameInfo.suffix());
			filename = increaseFilenameNumber(newFilenameInfo);
		}
		qDebug() << "saving file: " << filename;

		std::ofstream outFile;

		outFile.open(filename.toStdString(), std::ios::out | std::ios::binary);
		if (!outFile.good() || !outFile.is_open()) {
			//return kNkMAIDResult_UnexpectedError;
			qDebug() << "could not open file for writing!";
			return;
		}

		qDebug() << "writing " << currentFileFileInfo.ulTotalLength << " bytes";
		outFile.write(currentFileData->buffer, currentFileFileInfo.ulTotalLength);
		outFile.close();
	}();

	delete[] currentFileData->buffer;
	delete currentFileData;
	currentFileData = nullptr;

	// acquire next item object
	QFuture<bool> acquireFuture = QtConcurrent::run(this, &MaidFacade::acquireItemObjects);
	acquireFutureWatcher.setFuture(acquireFuture);
}

bool MaidFacade::toggleLiveView() {
	int32_t lvStatus = 0;

	if (isLiveViewActive()) {
		lvStatus = 0;
	} else {
		lvStatus = 1;
	}

	try {
		sourceObject->capSet(kNkMAIDCapability_LiveViewStatus, kNkMAIDDataType_Unsigned, (NKPARAM) lvStatus);

		//if (isLiveViewActive()) {
		//	getLiveViewImage();
		//}
	} catch (Maid::MaidError) {
		return false;
	}

	return true;
}

/**
 * throws MaidError
 */
bool MaidFacade::isLiveViewActive() {
	int32_t lvStatus = 0;
	sourceObject->capGet(kNkMAIDCapability_LiveViewStatus, kNkMAIDDataType_UnsignedPtr, (NKPARAM) &lvStatus);
	return lvStatus == 1;
}

/**
 * throws MaidError
 */
QImage MaidFacade::getLiveViewImage() {
	unsigned int headerSize = 0;
	NkMAIDArray dataArray;
	dataArray.pData = nullptr;
	int i = 0;
	std::unique_ptr<unsigned char[]> data;
	bool r = true;
	QImage empty = QImage();

	headerSize = 384;

	memset(&dataArray, 0, sizeof(NkMAIDArray));

	// check if everything is supported

	NkMAIDCapInfo capInfo;
	r = sourceObject->getCapInfo(kNkMAIDCapability_GetLiveViewImage, &capInfo);
	if (!r) {
		return empty;
	}

	r = sourceObject->hasCapOperation(kNkMAIDCapability_GetLiveViewImage, kNkMAIDCapOperation_Get);
	r = r && sourceObject->hasCapOperation(kNkMAIDCapability_GetLiveViewImage, kNkMAIDCapOperation_GetArray);
	if (!r) {
		return empty;
	}

	try {
		// get info about image, allocate memory
		sourceObject->capGet(kNkMAIDCapability_GetLiveViewImage, kNkMAIDDataType_ArrayPtr, (NKPARAM) &dataArray);
		dataArray.pData = new unsigned char[dataArray.ulElements * dataArray.wPhysicalBytes];
		data.reset((unsigned char*) dataArray.pData); // just for RAII/automatic deletion

		// get data
		sourceObject->capGetArray(kNkMAIDCapability_GetLiveViewImage, kNkMAIDDataType_ArrayPtr, (NKPARAM) &dataArray);
	} catch (Maid::MaidError) {
		return empty;
	}

	// construct a QImage out of the data and return it
	return QImage::fromData(((unsigned char*) dataArray.pData) + headerSize, dataArray.ulElements - headerSize, "JPEG");
}

std::pair<QStringList, size_t> MaidFacade::toQStringList(const StringValues& values) {
	QStringList list;
	for (auto& s : values.values) {
		list.append(QString::fromStdString(s));
	}
	return std::make_pair(list, values.currentValue);
}

NKERROR MaidFacade::processMaidData(NKREF ref, LPVOID info, LPVOID data) {
	NkMAIDDataInfo* dataInfo = static_cast<NkMAIDDataInfo*>(info);
	NkMAIDFileInfo* fileInfo = static_cast<NkMAIDFileInfo*>(info);
	NkMAIDImageInfo* imageInfo = static_cast<NkMAIDImageInfo*>(info);
	auto* r = static_cast<MaidFacade::DataProcData*>(ref);
	
	if (dataInfo->ulType & kNkMAIDDataObjType_File) {
		// initialize buffer
		if (r->offset == 0 && r->buffer == nullptr) {
			r->buffer = new char[fileInfo->ulTotalLength];
		}

		memcpy(r->buffer + r->offset, data, fileInfo->ulLength);
		r->offset += fileInfo->ulLength;

		if (r->offset >= fileInfo->ulTotalLength) {
			// file delivery is finished
			// the file will be written to disk in acquireItemObjectsFinished

			setCurrentFileData(r, info);
		}
	} else { // image
		return kNkMAIDResult_UnexpectedError;

		//unsigned long totalSize = imageInfo->ulRowBytes * imageInfo->szTotalPixels.h;
		//if (r->offset == 0 && r->buffer == nullptr) {
		//	r->buffer = new char[totalSize];
		//}
		//unsigned long byte = imageInfo->ulRowBytes * imageInfo->rData.h;
		//memcpy(r->buffer + r->offset, data, byte);
		//r->offset += byte;

		//if (r->offset >= totalSize) {
		//	std::string filename = makePictureFilename(dataInfo, nullptr);
		//	std::ofstream outFile;

		//	outFile.open(filename, std::ios::out | std::ios::binary);
		//	if (!outFile.good() || !outFile.is_open()) {
		//		return kNkMAIDResult_UnexpectedError;
		//	}

		//	outFile.write(r->buffer, totalSize);
		//	delete[] r->buffer;
		//	r->buffer = nullptr;
		//	r->offset = 0;
		//	outFile.close();
		//}
	}

	return kNkMAIDResult_NoError;
}

std::string MaidFacade::makePictureFilename() {
	std::string prefix;
	std::string ext;

	if (currentFileDataInfo.ulType & kNkMAIDDataObjType_Image) {
		prefix = "Image";
	} else if (currentFileDataInfo.ulType & kNkMAIDDataObjType_Thumbnail) {
		prefix = "Thumb";
	} else {
		prefix = "Unknown";
	}

	switch (currentFileFileInfo.ulFileDataType) {
	case kNkMAIDFileDataType_JPEG:
		ext = "jpg";
		break;
	case kNkMAIDFileDataType_TIFF:
		ext = "tif";
		break;
	case kNkMAIDFileDataType_NIF:
		ext = "nef";
		break;
	//case kNkMAIDFileDataType_NDF:
	//	ext = "ndf";
	//	break;
	default:
		ext = "dat";
	}

	std::stringstream filenameStream;
	filenameStream << prefix << "." << ext;

	return filenameStream.str();
}

/**
 * For image0.jpg, this will return image1.jpg, etc.
 */
QString MaidFacade::increaseFilenameNumber(const QFileInfo& fileInfo) {
	std::ifstream testFileIn;
	QString basePath = fileInfo.filePath().remove("." + fileInfo.completeSuffix());
	QString filename = "";
	// test file names
	while (true) {
		filename = basePath + "_" + QString::number(++prevFileNumber) + "." + fileInfo.completeSuffix();
		testFileIn.open(filename.toStdString());
		if (!testFileIn.good()) {
			testFileIn.close();
			break;
		}

		testFileIn.close();
	}

	return filename;
}

void MaidFacade::setCurrentFileData(DataProcData* fileData, void* info) {
	currentFileData = fileData;
	currentFileDataInfo = *static_cast<NkMAIDDataInfo*>(info);
	currentFileFileInfo = *static_cast<NkMAIDFileInfo*>(info);
}

void MaidFacade::progressCallbackUpdate(ULONG command, ULONG param, ULONG done, ULONG total) {
	if (command == kNkMAIDCommand_CapStart && param == kNkMAIDCapability_Acquire) {
		emit updateAcquireProgress(done, total);
	}
}

void CALLPASCAL CALLBACK eventProc(NKREF ref, ULONG eventType, NKPARAM data) {
	MaidFacade* maidFacade = (MaidFacade*) ref;

	switch (eventType) {
	case kNkMAIDEvent_AddChild:
		qDebug() << "A MAID child was added: " << data;
		maidFacade->startAcquireItemObjects();
		break;
	case kNkMAIDEvent_RemoveChild:
		qDebug() << "A MAID child was removed: " << data;
		break;
	case kNkMAIDEvent_WarmingUp:
		// The Type0007 Module does not use this event.
		break;
	case kNkMAIDEvent_WarmedUp:
		// The Type0007 Module does not use this event.
		break;
	case kNkMAIDEvent_CapChange:
		// TODO re-enumerate the capabilities
		maidFacade->capValueChangeCallback(data);
		break;
	case kNkMAIDEvent_CapChangeValueOnly:
		maidFacade->capValueChangeCallback(data);
		break;
	case kNkMAIDEvent_OrphanedChildren:
		// TODO close children (source objects)
		break;
	default:
		qDebug() << "Unknown event in a MaidObject.";
	}
}

NKERROR CALLPASCAL CALLBACK dataProc(NKREF ref, LPVOID info, LPVOID data) {
	return static_cast<MaidFacade::DataProcData*>(ref)->maidFacade->processMaidData(ref, info, data);
}

void CALLPASCAL CALLBACK completionProc(
		LPNkMAIDObject object,
		ULONG command,
		ULONG param,
		ULONG dataType,
		NKPARAM data,
		NKREF refComplete,
		NKERROR result) {

	auto complData = static_cast<MaidFacade::CompletionProcData*>(refComplete);
	complData->result = result;
	(*complData->count)++;

	// if the operation is aquire, free the memory
	if(command == kNkMAIDCommand_CapStart && param == kNkMAIDCapability_Acquire) {
		// here, complData->data would be deleted
		// because shoot (Acquire) is threaded, this is now done in acquireItemObjectsFinished
	}
	
	if (complData != nullptr) {
		delete complData;
	}
}

void CALLPASCAL CALLBACK progressProc(
		ULONG command,
		ULONG param, 
		NKREF ref, 
		ULONG done, 
		ULONG total) {

	static_cast<MaidFacade::ProgressProcData*>(ref)->maidFacade->progressCallbackUpdate(command, param, done, total);
}

#endif