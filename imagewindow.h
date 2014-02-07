#ifndef _IMAGEWINDOW_H
#define _IMAGEWINDOW_H

#include <SFML\Graphics.hpp>
#include "image.h"

class ImageWindow {
public:
	ImageWindow(const Image& image, bool decorate = true);		// Constructors
	ImageWindow(int width, int height, sf::Uint8* buf, bool decorate = true);
	ImageWindow(ImageWindow&& ref);			// Move constructor
	~ImageWindow();							// Destructor
	friend void swap(ImageWindow& first, ImageWindow& second) {
		using std::swap;
		swap(first._w, second._w);
		swap(first._t, second._t);
		swap(first._s, second._s);
		swap(first._col, second._col);
		swap(first._title, second._title);
		swap(first._width, second._width);
		swap(first._height, second._height);
		swap(first._zoom, second._zoom);
		swap(first._figNum, second._figNum);
		swap(first._showHistCDF, second._showHistCDF);
		swap(first._hist, second._hist);
		swap(first._cdf, second._cdf);
	}

	void update();	// Handles events and draws the window
	bool isOpen() const { return _w ? _w->isOpen() : false; }
	void setTitle(const std::string& title) { _title = title; updateTitle(); }
	void position(int x, int y) {
		if (!_w) return;
		_w->setPosition(sf::Vector2i(x, y));
	}
	void move(int dx, int dy) {
		if (!_w) return;
		sf::Vector2i pos = _w->getPosition();
		_w->setPosition(pos + sf::Vector2i(dx, dy));
	}
	void resize(int x, int y) {
		if (!_w) return;
		_w->setSize(sf::Vector2u(x, y));
	}
	void setColor(sf::Color col) { _col = col; }
	void setColor(sf::Uint8 r, sf::Uint8 g, sf::Uint8 b) { setColor(sf::Color(r, g, b, 255)); }
	void zoom(int times);
	void zoomIn(int times = 1) { zoom(times); }
	void zoomOut(int times = 1) { zoom(-times); }
	void showHistCDF(bool show) { _showHistCDF = show; }
private:
	// Disable copying
	ImageWindow(const ImageWindow& ref);		// Copy constructor -- NOT IMPLEMENTED
	ImageWindow& operator=(ImageWindow ref);	// Assignment operator -- NOT IMPLEMENTED

	// Members
	sf::RenderWindow* _w;	// The SFML window
	sf::Texture* _t;		// GFX memory of image
	sf::Sprite* _s;			// Object used to draw image
	sf::Color _col;			// Background color
	std::string _title;		// Optional title for image
	int _width, _height;	// Dimensions of the image
	double _zoom;			// Current zoom level
	int _figNum;			// Figure number
	bool _showHistCDF;		// Whether to show the histogram and CDF
	std::vector<sf::RectangleShape> _hist;	// Histogram rectangles
	sf::VertexArray _cdf;					// CDF
	// Static members
	static int _count;				// Number of windows
	static const double _offset;	// How much to translate image by
	static const double _zFactor;	// How much to zoom by

	// Utility methods
	void initImageBuf(int size, sf::Uint8* buf);	// Initialize image buffer
	void updateTitle();								// Update the window title
};

#endif