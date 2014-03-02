#ifndef _COMMON_H
#define _COMMON_H

#include <string>
#include <sstream>
#include <vector>
#include <list>
#include "image.h"
#include "imagewindow.h"

// Folder names
extern std::stringstream ss;
extern std::string topfolder;
extern std::string psffolder;
// Screen dimensions
static const int sx = 1680;
static const int sy = 0;
static const int sw = 1280;
static const int sh = 1024;

// Image windows
extern std::list<ImageWindow> windowList;
extern int rows;
extern int cols;

// Read and write images
Image readImage(const std::string& filename);
void readColorImage(const Image& r, const Image& g, const Image& b, const std::string& filename);
void writeImage(const Image& img, const std::string& filename);
void writeColorImage(const Image& r, const Image& g, const Image& b, const std::string& filename);

// Display images
ImageWindow* showImage(const Image& img, bool fullscreen,
	const std::string& title = "", int dx = 0, int dy = 0);
ImageWindow* showColorImage(const Image& r, const Image& g, const Image& b, bool fullscreen,
	const std::string& title = "", int dx = 0, int dy = 0);
void addImageToGrid(const Image& img, const std::string& title = "");
void showGrid();
// Process image window events
void loop();

#endif