/*******************************************************************************************************
 DkBaseViewPort.h
 Created on:	03.07.2013
 
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

#pragma once

#include <QGraphicsView>
#include <QCoreApplication>
#include <QTimer>
#include <QMovie>
#include <QShortcut>

#include <float.h>

// gestures
#include <QSwipeGesture>

#include "DkImageStorage.h"

#if QT_VERSION < 0x050000
// native gestures
#ifndef QT_NO_GESTURES
#include "extern/qevent_p.h"
#endif
#endif

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

class DllExport DkBaseViewPort : public QGraphicsView {
	Q_OBJECT

public:
	
	enum swipes{
		no_swipe = 0, // dummy for now
		next_image,
		prev_image,
		open_thumbs,
		close_thumbs,
		open_metadata,
		close_metadata,
		
		swipes_end
	};
	
	enum shortcuts{
		sc_pan_up,
		sc_pan_down,
		sc_pan_left,
		sc_pan_right,
		sc_zoom_in,
		sc_zoom_out,
		sc_zoom_in_alt,
		sc_zoom_out_alt,

		sc_end,
	};

	enum keys {
		shortcut_panning_left 	= Qt::CTRL + Qt::Key_Left,
		shortcut_panning_right 	= Qt::CTRL + Qt::Key_Right,
		shortcut_panning_up 	= Qt::CTRL + Qt::Key_Up,
		shortcut_panning_down 	= Qt::CTRL + Qt::Key_Down,

		shortcut_zoom_in 		= Qt::Key_Plus,
		shortcut_zoom_out		= Qt::Key_Minus,
		shortcut_zoom_in_alt	= Qt::Key_Up,
		shortcut_zoom_out_alt	= Qt::Key_Down,

	};
	
	DkBaseViewPort(QWidget *parent = 0, Qt::WindowFlags flags = 0);
	virtual ~DkBaseViewPort();

	virtual void release();
	void zoomConstraints(float minZoom = 0.01f, float maxZoom = 50.0f);
	virtual void zoom(float factor = 0.5, QPointF center = QPointF(-1,-1));
	void setForceFastRendering(bool fastRendering = true) {
		this->forceFastRendering = fastRendering;
	};
	
	/**
	 * Returns the scale factor for 100%.
	 * Note this factor is only valid for the current image.
	 * @return float 
	 **/ 
	float get100Factor() {
		
		updateImageMatrix();
		return 1.0f/imgMatrix.m11();
	};

	void setPanControl(QPointF panControl) {
		this->panControl = panControl;
	};
	
	virtual QTransform getWorldMatrix() { 
		return worldMatrix;
	};
	virtual QRect getMainGeometry() {
		return geometry();
	};

	QImage getCurrentImageRegion();

	virtual DkImageStorage* getImageStorage() {
		return &imgStorage;
	};

	//virtual QImage getScaledImage(float factor);

#ifdef WITH_OPENCV
	virtual void setImage(cv::Mat newImg);
#endif

	virtual QImage getImage();

	virtual QRectF getImageViewRect();

	virtual bool imageInside();	// always return false?!


signals:
	void enableNoImageSignal(bool enable);
	void showStatusBar(bool show, bool permanent);
	void imageUpdated();	// this waits ~50 ms before triggering
	void newImageSignal(QImage* img);
	
//#ifdef DK_DLL
	void keyReleaseSignal(QKeyEvent* event);	// make key presses available
//#endif

public slots:
	virtual void togglePattern(bool show);
	virtual void shiftLeft();
	virtual void shiftRight();
	virtual void shiftUp();
	virtual void shiftDown();
	virtual void moveView(QPointF);
	virtual void zoomIn();
	virtual void zoomOut();
	virtual void resetView();
	virtual void fullView();
	virtual void resizeEvent(QResizeEvent* event);
	virtual void stopBlockZooming();
	//virtual void setBackgroundColor();
	virtual void setBackgroundBrush(const QBrush &brush);

	virtual bool unloadImage(bool fileChange = true);

	virtual void setImage(QImage newImg);


protected:
	virtual bool event(QEvent *event);
	virtual void keyPressEvent(QKeyEvent *event);
	virtual void keyReleaseEvent(QKeyEvent *event);
	virtual void mousePressEvent(QMouseEvent *event);
	virtual void mouseReleaseEvent(QMouseEvent *event);
	virtual void mouseMoveEvent(QMouseEvent *event);
	virtual void wheelEvent(QWheelEvent *event);
	virtual void mouseDoubleClickEvent(QMouseEvent *event);
	virtual void contextMenuEvent(QContextMenuEvent *event);
	virtual void paintEvent(QPaintEvent* event);

#ifndef QT_NO_GESTURES
	virtual bool nativeGestureEvent(QNativeGestureEvent* event);
	virtual int swipeRecognition(QNativeGestureEvent* event) { return no_swipe;};	// dummy
#endif
	virtual bool gestureEvent(QGestureEvent* event);
	virtual void swipeAction(int swipeGesture) {};

	QVector<QShortcut*> shortcuts;	

	QPainter* painter;
	Qt::KeyboardModifier altMod;		// it makes sense to switch these modifiers on linux (alt + mouse moves windows there)
	Qt::KeyboardModifier ctrlMod;

	QWidget *parent;
	//QImage imgQt;
	//QMap<int, QImage> imgPyramid;
	DkImageStorage imgStorage;
	QMovie* movie;
	QBrush pattern;


	QTransform imgMatrix;
	QTransform worldMatrix;
	QRectF imgViewRect;
	QRectF viewportRect;
	QRectF imgRect;

	QPointF panControl;	// controls how far we can pan outside an image
	QPointF posGrab;
	float minZoom;
	float maxZoom;
	float lastZoom;
	float startZoom;
	int swipeGesture;

	bool forceFastRendering;
	bool blockZooming;
	QTimer* zoomTimer;

	// functions
	virtual void draw(QPainter *painter, float opacity = 1.0f);
	virtual void updateImageMatrix();
	virtual QTransform getScaledImageMatrix();
	virtual void controlImagePosition(float lb = -1, float ub = -1);
	virtual void centerImage();
	virtual void changeCursor();
	virtual void createShortcuts();

};

};
