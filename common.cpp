#include <memory>
#include "common.h"

// Folder names
std::stringstream ss;
std::string topfolder = "";
std::string psffolder = "psf/";

// Image windows
std::list<ImageWindow> windowList;
// Grid of image windows
std::vector<Image> imageVec;
std::vector<std::string> titleVec;
int rows = 1, cols = 1, imw = 120, imh = 0;

// Read and write images
Image readImage(const std::string& filename) {
	sf::Image img; img.loadFromFile(filename);
	Image out; out.fromUchar(img.getSize().x, img.getSize().y, 4, 0x7, img.getPixelsPtr());
	return out;
}
void readColorImage(Image& r, Image& g, Image& b, const std::string& filename) {
	sf::Image img; img.loadFromFile(filename);
	r.fromUchar(img.getSize().x, img.getSize().y, 4, 0x1, img.getPixelsPtr());
	g.fromUchar(img.getSize().x, img.getSize().y, 4, 0x2, img.getPixelsPtr());
	b.fromUchar(img.getSize().x, img.getSize().y, 4, 0x4, img.getPixelsPtr());
}
void writeImage(const Image& img, const std::string& filename) {
	std::vector<sf::Uint8> buf; buf.resize(img.getArraySize() * 4);
	img.toUchar(4, 0x7, buf.data(), true);

	sf::Image saveImg; saveImg.create(img.getWidth(), img.getHeight(), buf.data());
	saveImg.saveToFile(filename);
}
void writeColorImage(const Image& r, const Image& g, const Image& b, const std::string& filename) {
	std::vector<sf::Uint8> buf; buf.resize(r.getArraySize() * 4);
	r.toUchar(4, 0x1, buf.data(), true);
	g.toUchar(4, 0x2, buf.data(), false);
	b.toUchar(4, 0x4, buf.data(), false);

	sf::Image saveImg; saveImg.create(r.getWidth(), r.getHeight(), buf.data());
	saveImg.saveToFile(filename);
}

// Display images
ImageWindow* showImage(const Image& img, bool fullscreen, const std::string& title, int dx, int dy) {
	std::unique_ptr<ImageWindow> w;
	if (!fullscreen) {
		// Create the image window
		w = std::unique_ptr<ImageWindow>(new ImageWindow(img, true));
		w->setTitle(title);
		w->move(dx, dy);
	} else {
		// Create an undecorated window and set its size and position to match the display
		w = std::unique_ptr<ImageWindow>(new ImageWindow(img, false));
		w->position(sx, sy);
		w->resize(sw, sh);
		w->setColor(0, 0, 0);
		w->setBCs(BC_PERIODIC);
		w->showBCs(true);
	}

	// Add window to the list and return a pointer to it
	w->update();
	windowList.push_back(std::move(*w));
	return &windowList.back();
}
ImageWindow* showColorImage(const Image& r, const Image& g, const Image& b, bool fullscreen,
	const std::string& title, int dx, int dy) {
	// Convert images into a color SFML image
	std::vector<sf::Uint8> buf; buf.resize(r.getArraySize() * 4);
	r.toUchar(4, 0x1, buf.data(), true);
	g.toUchar(4, 0x2, buf.data(), false);
	b.toUchar(4, 0x4, buf.data(), false);

	std::unique_ptr<ImageWindow> w;
	if (!fullscreen) {
		// Create the image window
		w = std::unique_ptr<ImageWindow>(new ImageWindow(r.getWidth(), r.getHeight(), buf.data(), true));
		w->setTitle(title);
		w->move(dx, dy);
	} else {
		// Create an undecorated window and set its size and position to match the display
		w = std::unique_ptr<ImageWindow>(new ImageWindow(r.getWidth(), r.getHeight(), buf.data(), false));
		w->position(sx, sy);
		w->resize(sw, sh);
		w->setColor(0, 0, 0);
		w->setBCs(BC_PERIODIC);
		w->showBCs(true);
	}

	// Add window to the list and return a pointer to it
	w->update();
	windowList.push_back(std::move(*w));
	return &windowList.back();
}
void addImageToGrid(const Image& img, const std::string& title) {
	// Store the image and title in lists
	if (img.getWidth() > imw) imw = img.getWidth();
	if (img.getHeight() > imh) imh = img.getHeight();
	imageVec.push_back(img);
	titleVec.push_back(title);
}
void showGrid() {
	std::reverse(imageVec.begin(), imageVec.end());
	std::reverse(titleVec.begin(), titleVec.end());

	// Create the windows
	for (int r = 0; r < rows; r++) {
		for (int c = 0; c < cols; c++) {
			if (imageVec.empty()) break;
			int x = (imw + 28) * (c - cols / 2) + ((cols + 1) % 2) * (imw / 2 + 14);
			int y = (imh + 48) * (r - rows / 2) + ((rows + 1) % 2) * (imh / 2 + 24) - 30;
			showImage(imageVec.back(), false, titleVec.back(), x, y);
			imageVec.pop_back(); titleVec.pop_back();
		}
	}
}
// Process window events
void loop() {
	while (!windowList.empty()) {
		for (auto it = windowList.begin(); it != windowList.end();) {
			it->update();

			if (!it->isOpen()) {
				// Destroy all windows if a single one is closed
				windowList.clear();
				break;
			} else it++;
		}
	}
}