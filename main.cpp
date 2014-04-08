#include <iostream>
#include <iomanip>
#include "precorrection.h"
#include "common.h"
#include "camcalib.h"

int main() {
	std::string psffolder = "psf/";
	//std::string path = "//matrix.cs.purdue.edu/CGVLab/may5/precorrection/Hamlet/ignacio/";
	std::string path = "C:/Users/Chris/Pictures/Precorrection/Hamlet/tablet/";
	std::string iterfolder = "iter/";
	std::string img = "text";
	bool color = false;
	sf::err().rdbuf(NULL);

	ss << path << img << ".png";
	Image r, g, b;
	if (color) { readColorImage(r, g, b, ss.str()); ss.str(""); }
	else { readImage(r, ss.str()); ss.str(""); }

	double dpt = 2.0;
	PSFParm ppm(dpt, 6.0, 0.5, 0.0);
	ss << psffolder << "psf_" << ppm.paramStr << ".dbl";
	Image psf; psf.fromBinary(ss.str()); ss.str("");

	Image r_conv, g_conv, b_conv;
	r_conv = convolve(r, psf, BC_PERIODIC);
	if (color) {
		g_conv = convolve(g, psf, BC_PERIODIC);
		b_conv = convolve(b, psf, BC_PERIODIC);
	}
	ss << path << img << "_conv_" << dpt << "d.png";
	if (color) { writeColorImage(r_conv, g_conv, b_conv, ss.str()); ss.str(""); }
	else { writeImage(r_conv, ss.str()); ss.str(""); }

	std::vector<std::pair<double, double>> contrasts;
	contrasts.push_back(std::pair<double, double>(0.4, 0.1));
	for (auto contrast = contrasts.begin(); contrast != contrasts.end(); contrast++) {
		int cnt_i = std::distance(contrasts.begin(), contrast) + 1;
		int cnt_max = std::distance(contrasts.begin(), contrasts.end());

		Image r_contrast, g_contrast, b_contrast;
		r_contrast = r.contrast(contrast->first) + contrast->second;
		if (color) {
			g_contrast = g.contrast(contrast->first) + contrast->second;
			b_contrast = b.contrast(contrast->first) + contrast->second;
		}
		ss << path << img << "_" << contrast->first << "c_" << contrast->second << "o.png";
		if (color) { writeColorImage(r_contrast, g_contrast, b_contrast, ss.str()); ss.str(""); }
		else { writeImage(r_contrast, ss.str()); ss.str(""); }

		Image r_contrast_conv, g_contrast_conv, b_contrast_conv;
		r_contrast_conv = convolve(r_contrast, psf, BC_PERIODIC);
		if (color) {
			g_contrast_conv = convolve(g_contrast, psf, BC_PERIODIC);
			b_contrast_conv = convolve(b_contrast, psf, BC_PERIODIC);
		}
		ss << path << img << "_conv_" << dpt << "d_" << contrast->first << "c_" << contrast->second << "o.png";
		if (color) { writeColorImage(r_contrast_conv, g_contrast_conv, b_contrast_conv, ss.str()); ss.str(""); }
		else { writeImage(r_contrast_conv, ss.str()); ss.str(""); }

		Options opts; opts.tv = TVL2; opts.relchg = DBL_EPSILON; opts.maxiter = 150;
		std::string tvln = opts.tv == TVL1 ? "L1" : "L2";
		std::vector<double> thetas;
		//thetas.push_back(6.4e3);
		//thetas.push_back(1.28e4);
		//thetas.push_back(2.56e4);
		//thetas.push_back(5.12e4);
		thetas.push_back(1.024e5);
		//thetas.push_back(2.048e5);
		for (auto theta = thetas.begin(); theta != thetas.end(); theta++) {
			int theta_i = std::distance(thetas.begin(), theta) + 1;
			int theta_max = std::distance(thetas.begin(), thetas.end());
			std::cout << cnt_i << " / " << cnt_max << "\t" << theta_i << " / " << theta_max << std::endl;

			ss << contrast->first << "c_" << contrast->second << "o_" << dpt << "d_" << *theta << "t";
			std::string paramstr = ss.str(); ss.str("");

			bool force = true;
			Image r_prec, g_prec, b_prec;
			std::vector<Iteration> r_hist, g_hist, b_hist;
			ss << path << img << "_prec" << tvln << "_" << paramstr <<  ".png";
			if ((color && !readColorImage(r_prec, g_prec, b_prec, ss.str())) ||
				(!color && !readImage(r_prec, ss.str())) || force) {
				r_prec = precorrect(r_contrast, psf, *theta, opts, NULL);
				if (color) {
					g_prec = precorrect(g_contrast, psf, *theta, opts, NULL);
					b_prec = precorrect(b_contrast, psf, *theta, opts, NULL);
				}
				if (color) { writeColorImage(r_prec, g_prec, b_prec, ss.str()); }
				else { writeImage(r_prec, ss.str()); }
			}
			ss.str("");

			std::cout << "RTV: " << Image::RTV(r, r_prec) << std::endl;

			Image r_prec_conv, g_prec_conv, b_prec_conv;
			ss << path << img << "_prec" << tvln << "_conv_" << paramstr << ".png";
			if ((color && !readColorImage(r_prec_conv, g_prec_conv, b_prec_conv, ss.str())) ||
				(!color && !readImage(r_prec_conv, ss.str())) || force) {
				r_prec_conv = convolve(r_prec, psf, BC_PERIODIC);
				if (color) {
					g_prec_conv = convolve(g_prec, psf, BC_PERIODIC);
					b_prec_conv = convolve(b_prec, psf, BC_PERIODIC);
				}
				if (color) { writeColorImage(r_prec_conv, g_prec_conv, b_prec_conv, ss.str()); }
				else { writeImage(r_prec_conv, ss.str()); }
			}
			ss.str("");

			// Save iterations
			for (int it = 0; it < r_hist.size(); it++) {
				ss << path << iterfolder << img << "_prec" << tvln << "_" << paramstr << "_" << it+1 << "i.png";
				if (color) { writeColorImage(r_hist[it].X, g_hist[it].X, b_hist[it].X, ss.str()); ss.str(""); }
				else { writeImage(r_hist[it].X, ss.str()); ss.str(""); }

				Image r_prec_conv_i, g_prec_conv_i, b_prec_conv_i;
				r_prec_conv_i = convolve(r_hist[it].X, psf, BC_PERIODIC);
				if (color) {
					g_prec_conv_i = convolve(g_hist[it].X, psf, BC_PERIODIC);
					b_prec_conv_i = convolve(b_hist[it].X, psf, BC_PERIODIC);
				}
				ss << path << iterfolder << img << "_prec" << tvln << "_conv_" << paramstr << "_" << it+1 << "i.png";
				if (color) { writeColorImage(r_prec_conv_i, g_prec_conv_i, b_prec_conv_i, ss.str()); ss.str(""); }
				else { writeImage(r_prec_conv_i, ss.str()); ss.str(""); }
			}
		}
	}

	return 0;
}

#if 0
int main(void) {
	topfolder = "thetaVsRTV/";
	psffolder = "psf/";
	std::string precfolder = "prec/";
	std::string imgprefix = "lenna";

	// Load the test image
	ss << imgfolder << imgprefix << ".png";
	Image F; readImage(F, ss.str()); ss.str("");
	// Reduce contrast to 70%
	//F = F.contrast(0.7);

	// Load the PSF
	PSFParm ppm(2.00, 6.00, 0.50, 0.0);
	ss << psffolder << "psf_" << ppm.paramStr << ".dbl";
	Image psf; psf.fromBinary(ss.str()); ss.str("");

	Options optsL1;
	optsL1.tv = TVL2;
	// Enforce 30 iterations
	optsL1.relchg = DBL_EPSILON;
	optsL1.maxiter = 30;

	double thetaMin = 5e3;
	double thetaMax = 5e3;
	double thetaInc = 8e2;
	int i_theta_max = (int)((thetaMax + thetaInc - thetaMin) / thetaInc + 0.5);

	std::vector<double> thetas;
	std::vector<double> rtvs;
	for (int i = 0; i < i_theta_max; i++) {
		double theta = i * thetaInc + thetaMin;
		thetas.push_back(theta);
	}
	
	// Disable SFML error output
	std::streambuf* prevBuf = sf::err().rdbuf(NULL);

	// Open log file
	ss << topfolder << imgprefix << "_L2_70.m";
	FILE* logfp = fopen(ss.str().c_str(), "wb"); ss.str("");
	if (!logfp) std::cout << "Couldn't open log file!" << std::endl;
	fprintf(logfp, "m2 = [\n");

	// Open prec log file
	ss << topfolder << imgprefix << "_prec_log.m";
	logfname = ss.str(); ss.str("");
	logfile = fopen(logfname.c_str(), "wb");
	if (logfile) {
		fprintf(logfile, "m = [\n");
	}
	
	bool force = true;
	sf::VertexArray vaL1(sf::LinesStrip, thetas.size());
	for (int i = 0; i < thetas.size(); i++) {
		std::cout << i+1 << " of " << thetas.size() << std::endl;
		ss << topfolder << precfolder << imgprefix << "_prec_L1_"
			<< std::setprecision(2) << std::fixed << thetas[i] << "t.png";
		std::string precL2name = ss.str(); ss.str("");

		Image precL2;
		if (!readImage(precL2, precL2name) || force) {
			precL2 = precorrect(F, psf, thetas[i], optsL1);
			writeImage(precL2, precL2name);
		}

		double rtvL2 = Image::RTV(F, precL2);
		rtvs.push_back(rtvL2);

		vaL1[i].color = sf::Color::Cyan;
		vaL1[i].position = sf::Vector2f(thetas[i], rtvL2);

		// Write to log file
		fprintf(logfp, "%f %f\n", thetas[i], rtvL2);
	}

	// Close prec log file
	if (logfile) {
		fprintf(logfile, "];\n");
		fclose(logfile);
	}

	// Close log file
	fprintf(logfp, "];\n");
	fclose(logfp);

	// Re-enable SFML error output
	sf::err().rdbuf(prevBuf);

	// Find mins and maxes
	double minX = *(std::min_element(thetas.begin(), thetas.end()));
	double maxX = *(std::max_element(thetas.begin(), thetas.end()));
	double minY = *(std::min_element(rtvs.begin(), rtvs.end()));
	double maxY = *(std::max_element(rtvs.begin(), rtvs.end()));
	double w = maxX - minX;
	double h = maxY - minY;

	// Flip vertically so origin is at lower-left
	sf::Transform t = sf::Transform::Identity;
	t.scale(1.0, -1.0, (minX + maxX) / 2.0, (minY + maxY) / 2.0);

	std::cout << "Min theta: " << minX << std::endl
		<< "Max theta: " << maxX << std::endl
		<< "Min rtv: " << minY << std::endl
		<< "Max rtv: " << maxY << std::endl;


	sf::RenderWindow win(sf::VideoMode(512, 512), "Theta vs. RTV");
	win.setVerticalSyncEnabled(true);
	sf::View v(sf::FloatRect(minX - w / 5.0, minY - h / 5.0, w * 7.0 / 5.0, h * 7.0 / 5.0));
	win.setView(v);

	while (win.isOpen()) {
		sf::Event e;
		while (win.pollEvent(e)) {
			switch (e.type) {
			case sf::Event::Closed:
				win.close(); break;
			case sf::Event::KeyReleased:
				if (e.key.code == sf::Keyboard::Escape)
					win.close();
				break;
			}
		}

		win.clear();
		win.draw(vaL1, t);
		win.display();
	}

	return 0;
}
#endif