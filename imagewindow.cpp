#include "imagewindow.h"

int ImageWindow::_count = 0;
const double ImageWindow::_offset = 20.0;
const double ImageWindow::_zFactor = 0.75;

ImageWindow::ImageWindow(const Image& image, bool decorate) {
	_width = image.getWidth();
	_height = image.getHeight();
	_zoom = 1.0;
	_figNum = ++_count;

	// Set window style parameters
	sf::Uint32 style;
	if (decorate) style = sf::Style::Default;
	else style = sf::Style::None;

	// Open a window
	_w = new sf::RenderWindow;
	_w->create(sf::VideoMode(_width, _height), "null", style);
	_w->setVerticalSyncEnabled(true);
	updateTitle();
	// Make sure view is centered
	sf::View v = _w->getView();
	v.setCenter(_width / 2, _height / 2);
	_w->setView(v);

	// Convert Image to RGBA8
	sf::Uint8* buf = new sf::Uint8 [image.getArraySize() * 4];
	image.toUchar(4, 0x7, buf, true);

	// Load image into GFX memory
	_t = new sf::Texture;
	_t->create(_width, _height);
	_t->update(buf);
	_s = new sf::Sprite;
	_s->setTexture(*_t, true);

	_col = sf::Color(128, 128, 128, 255);

	delete [] buf; buf = NULL;
}
ImageWindow::ImageWindow(int width, int height, sf::Uint8* buf, bool decorate) {
	_width = width;
	_height = height;
	_zoom = 1.0;
	_figNum = ++_count;

	// Set window style parameters
	sf::Uint32 style;
	if (decorate) style = sf::Style::Default;
	else style = sf::Style::None;

	// Open a window
	_w = new sf::RenderWindow;
	_w->create(sf::VideoMode(_width, _height), "null", style);
	_w->setVerticalSyncEnabled(true);
	updateTitle();
	// Make sure view is centered
	sf::View v = _w->getView();
	v.setCenter(_width / 2, _height / 2);
	_w->setView(v);

	// Load image into GFX memory
	_t = new sf::Texture;
	_t->create(_width, _height);
	_t->update(buf);
	_s = new sf::Sprite;
	_s->setTexture(*_t, true);

	_col = sf::Color(128, 128, 128, 255);
}
ImageWindow::ImageWindow(ImageWindow&& ref) {
	_w = NULL;
	_t = NULL;
	_s = NULL;
	swap(*this, ref);
}
ImageWindow::~ImageWindow() {
	if (_w) delete _w;
	if (_t) delete _t;
	if (_s) delete _s;
}

void ImageWindow::update() {
	if (!_w) return;

	// Poll for events
	sf::Event e;
	while (_w->pollEvent(e)) {
		sf::View v;		// Used when events manipulate the view
		switch (e.type) {
		// User closes the window
		case sf::Event::Closed:
			_w->close();
			break;
		// Window is resized
		case sf::Event::Resized:
			v = _w->getView();
			v.setSize((float)e.size.width, (float)e.size.height);
			v.zoom(_zoom);
			_w->setView(v);
			break;
		case sf::Event::KeyReleased:
			switch (e.key.code) {
			// Escape also closes the window
			case sf::Keyboard::Escape:
				_w->close();
				break;
			// Zoom in and out of the image
			case sf::Keyboard::Add:
				zoomIn();
				break;
			case sf::Keyboard::Subtract:
				zoomOut();
				break;
			}
			break;
		case sf::Event::KeyPressed:
			switch (e.key.code) {
			// Translate image left, right, up, or down
			case sf::Keyboard::Left:
				v = _w->getView();
				v.setCenter(v.getCenter() + sf::Vector2f(-_offset * _zoom * (1.0 + 2.0 * e.key.shift), 0.0));
				_w->setView(v);
				break;
			case sf::Keyboard::Right:
				v = _w->getView();
				v.setCenter(v.getCenter() + sf::Vector2f(_offset * _zoom * (1.0 + 2.0 * e.key.shift), 0.0));
				_w->setView(v);
				break;
			case sf::Keyboard::Up:
				v = _w->getView();
				v.setCenter(v.getCenter() + sf::Vector2f(0.0, -_offset * _zoom * (1.0 + 2.0 * e.key.shift)));
				_w->setView(v);
				break;
			case sf::Keyboard::Down:
				v = _w->getView();
				v.setCenter(v.getCenter() + sf::Vector2f(0.0, _offset * _zoom * (1.0 + 2.0 * e.key.shift)));
				_w->setView(v);
				break;
			}
			break;
		}
	}

	// Clear display
	_w->clear(_col);
	// Draw sprite
	_w->draw(*_s);
	// Swap buffers
	_w->display();
}
void ImageWindow::zoom(int times) {
	if (!_w) return;
	sf::View v = _w->getView();
	double z = pow(_zFactor, times);
	v.zoom(z);
	_zoom *= z;
	_w->setView(v);
	_w->setSize(sf::Vector2u(_width / _zoom, _height / _zoom));
	updateTitle();
}

void ImageWindow::updateTitle() {
	if (!_w) return;

	std::stringstream ss;
	ss << "Figure " << _figNum;
	if (!_title.empty())
		ss << ": " << _title;
	ss << " -- " << 100.0 / _zoom << "%";
	std::string title = ss.str();

	_w->setTitle(title);
}