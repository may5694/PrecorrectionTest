#ifndef _IMAGEWINDOW_H
#define _IMAGEWINDOW_H

#include <SFML\Graphics.hpp>
#include "image.h"

class ImageWindow {
public:
	ImageWindow();							// Default constructor
	ImageWindow(const Image& image);		// Constructor
	ImageWindow(int width, int height, sf::Uint8* buf);
	ImageWindow(ImageWindow&& ref);			// Move constructor
	~ImageWindow();							// Destructor
	friend void swap(ImageWindow& first, ImageWindow& second) {
		using std::swap;
		swap(first._w, second._w);
		swap(first._t, second._t);
		swap(first._s, second._s);
		swap(first._title, second._title);
		swap(first._width, second._width);
		swap(first._height, second._height);
		swap(first._zoom, second._zoom);
		swap(first._figNum, second._figNum);
	}

	void update();	// Handles events and draws the window
	bool isOpen() const { return _w ? _w->isOpen() : false; }
	void setTitle(const std::string& title) { _title = title; updateTitle(); }
	void move(int dx, int dy) {
		if (!_w) return;
		sf::Vector2i pos = _w->getPosition();
		_w->setPosition(pos + sf::Vector2i(dx, dy));
	}
	void zoom(int times);
	void zoomIn(int times = 1) { zoom(times); }
	void zoomOut(int times = 1) { zoom(-times); }
private:
	// Disable copying
	ImageWindow(const ImageWindow& ref);		// Copy constructor -- NOT IMPLEMENTED
	ImageWindow& operator=(ImageWindow ref);	// Assignment operator -- NOT IMPLEMENTED

	// Members
	sf::RenderWindow* _w;	// The SFML window
	sf::Texture* _t;		// GFX memory of image
	sf::Sprite* _s;			// Object used to draw image
	std::string _title;		// Optional title for image
	int _width, _height;	// Dimensions of the image
	double _zoom;			// Current zoom level
	int _figNum;			// Figure number
	// Static members
	static int _count;				// Number of windows
	static const double _offset;	// How much to translate image by
	static const double _zFactor;	// How much to zoom by

	// Utility methods
	void initImageBuf(int size, sf::Uint8* buf);	// Initialize image buffer
	void updateTitle();								// Update the window title
};

#endif