#include <iostream>
#include <fstream>
#include <iomanip>
#include <functional>
#include "precorrection.h"
#include "common.h"
#include "camcalib.h"

// ZBrent finds the bracketed root of a 1D function
extern float zbrent(std::function<float(float)> func, float x1, float x2, float tol);

int main() {
	std::string psffolder = "psf/";
	//std::string path = "//matrix.cs.purdue.edu/CGVLab/may5/precorrection/Hamlet/ignacio/";
	std::string path = "C:/Users/Chris/Pictures/Precorrection/disc/";
	std::string iterfolder = "iter/";
	std::string thetafolder = "theta/";
	std::string img = "disc";
	bool color = false;
	sf::err().rdbuf(NULL);

	// Load image
	ss << path << img << ".png";
	Image r, g, b;
	if (color) { readColorImage(r, g, b, ss.str()); ss.str(""); }
	else { readImage(r, ss.str()); ss.str(""); }

	// Load PSF
	double dpt = 2.0;
	PSFParm ppm(dpt, 6.0, 0.5, 0.0);
	ss << psffolder << "psf_" << ppm.paramStr << ".dbl";
	Image psf; psf.fromBinary(ss.str()); ss.str("");

	// Initial convolution
	Image r_conv, g_conv, b_conv;
	r_conv = convolve(r, psf, BC_PERIODIC);
	if (color) {
		g_conv = convolve(g, psf, BC_PERIODIC);
		b_conv = convolve(b, psf, BC_PERIODIC);
	}
	ss << path << img << "_conv_" << dpt << "d.png";
	if (color) { writeColorImage(r_conv, g_conv, b_conv, ss.str()); ss.str(""); }
	else { writeImage(r_conv, ss.str()); ss.str(""); }

	ss << path << img << "_log.txt";
	std::ofstream log(ss.str()); ss.str("");
	ss << path << img << "_ml.m";
	std::ofstream ml(ss.str()); ss.str("");

	log << "Name\tDiopters\tContrast\tRTV\tTheta" << std::endl;
	ml << "% contrast, rtv, theta" << std::endl
		<< "m = [" << std::endl;

	// Loop through contrasts
	std::vector<std::pair<double, double>> contrasts;
	contrasts.push_back(std::pair<double, double>(1.0, 0.0));
	contrasts.push_back(std::pair<double, double>(0.8, 0.0));
	contrasts.push_back(std::pair<double, double>(0.6, 0.0));
	contrasts.push_back(std::pair<double, double>(0.4, 0.0));
	contrasts.push_back(std::pair<double, double>(0.2, 0.0));
	for (auto contrast = contrasts.begin(); contrast != contrasts.end(); contrast++) {
		int cnt_i = std::distance(contrasts.begin(), contrast) + 1;
		int cnt_max = std::distance(contrasts.begin(), contrasts.end());

		// Reduce contrast
		Image r_contrast, g_contrast, b_contrast;
		r_contrast = r.contrast(contrast->first) + contrast->second;
		if (color) {
			g_contrast = g.contrast(contrast->first) + contrast->second;
			b_contrast = b.contrast(contrast->first) + contrast->second;
		}
		ss << path << img << "_" << contrast->first << "c_" << contrast->second << "o.png";
		if (color) { writeColorImage(r_contrast, g_contrast, b_contrast, ss.str()); ss.str(""); }
		else { writeImage(r_contrast, ss.str()); ss.str(""); }

		// Convolve contrast-reduced image
		Image r_contrast_conv, g_contrast_conv, b_contrast_conv;
		r_contrast_conv = convolve(r_contrast, psf, BC_PERIODIC);
		if (color) {
			g_contrast_conv = convolve(g_contrast, psf, BC_PERIODIC);
			b_contrast_conv = convolve(b_contrast, psf, BC_PERIODIC);
		}
		ss << path << img << "_conv_" << dpt << "d_" << contrast->first << "c_" << contrast->second << "o.png";
		if (color) { writeColorImage(r_contrast_conv, g_contrast_conv, b_contrast_conv, ss.str()); ss.str(""); }
		else { writeImage(r_contrast_conv, ss.str()); ss.str(""); }

		// Loop through desired RTV
		std::vector<float> rtvTargets({ 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0 });
		for (auto targetRTV = rtvTargets.begin(); targetRTV != rtvTargets.end(); ++targetRTV) {

			// Precorrection options
			bool force = false;
			Options opts; opts.tv = TVL2; opts.relchg = DBL_EPSILON; opts.maxiter = 30;
			std::string tvln = opts.tv == TVL1 ? "L1" : "L2";
			// Theta search parameters
			float minTheta = 1.25e1f, maxTheta = 1e5f, tol = 1e-2f;

			// Function to try out a theta value
			std::function<float(float)> tryTheta = [&](float theta) -> float {
				std::cout << "Theta: " << theta << std::endl;

				ss << contrast->first << "c_" << contrast->second << "o_" << dpt << "d_" << theta << "t";
				std::string paramstr = ss.str(); ss.str("");
				ss << path << thetafolder << img << "_prec" << tvln << "_" << paramstr << ".png";
				std::string precname = ss.str(); ss.str("");

				// Precorrect image using this theta
				Image r_prec, g_prec, b_prec;
				if ((color && !readColorImage(r_prec, g_prec, b_prec, precname)) ||
					(!color && !readImage(r_prec, precname)) || force) {
					r_prec = precorrect(r_contrast, psf, theta, opts, NULL);
					if (color) {
						g_prec = precorrect(g_contrast, psf, theta, opts, NULL);
						b_prec = precorrect(b_contrast, psf, theta, opts, NULL);
					}
					if (color) { writeColorImage(r_prec, g_prec, b_prec, precname); } else { writeImage(r_prec, precname); }
				}

				// Return the RTV difference from target RTV
				float rtv = Image::RTV(r_contrast, r_prec);
				std::cout << "RTV: " << rtv << std::endl;
				return rtv - *targetRTV;
			};

			// Search for a theta giving us the desired RTV
			float theta = zbrent(tryTheta, minTheta, maxTheta, tol);

			std::cout << std::endl << "Final theta: " << theta << std::endl;

			ss << contrast->first << "c_" << contrast->second << "o_" << dpt << "d_" << *targetRTV << "r";
			std::string paramstr = ss.str(); ss.str("");

			// Precorrect
			Image r_prec, g_prec, b_prec;
			std::vector<Iteration> r_hist, g_hist, b_hist;
			ss << path << img << "_prec" << tvln << "_" << paramstr << ".png";
			if ((color && !readColorImage(r_prec, g_prec, b_prec, ss.str())) ||
				(!color && !readImage(r_prec, ss.str())) || force) {
				r_prec = precorrect(r_contrast, psf, theta, opts, NULL);
				if (color) {
					g_prec = precorrect(g_contrast, psf, theta, opts, NULL);
					b_prec = precorrect(b_contrast, psf, theta, opts, NULL);
				}
				if (color) { writeColorImage(r_prec, g_prec, b_prec, ss.str()); } else { writeImage(r_prec, ss.str()); }
			}
			ss.str("");

			double RTV = Image::RTV(r_contrast, r_prec);
			std::cout << "RTV: " << RTV << std::endl;

			// Convolve precorrected
			Image r_prec_conv, g_prec_conv, b_prec_conv;
			ss << path << img << "_prec" << tvln << "_conv_" << paramstr << ".png";
			if ((color && !readColorImage(r_prec_conv, g_prec_conv, b_prec_conv, ss.str())) ||
				(!color && !readImage(r_prec_conv, ss.str())) || force) {
				r_prec_conv = convolve(r_prec, psf, BC_PERIODIC);
				if (color) {
					g_prec_conv = convolve(g_prec, psf, BC_PERIODIC);
					b_prec_conv = convolve(b_prec, psf, BC_PERIODIC);
				}
				if (color) { writeColorImage(r_prec_conv, g_prec_conv, b_prec_conv, ss.str()); } else { writeImage(r_prec_conv, ss.str()); }
			}
			ss.str("");

			log << img << "\t" << dpt << "\t[" << (0.5 - contrast->first * 0.5) + contrast->second << ", " << (0.5 + contrast->first * 0.5) + contrast->second
				<< "] " << contrast->first * 100 << "%\t" << RTV << "\t" << theta << std::endl;
			ml << contrast->first << " " << RTV << " " << theta << ";" << std::endl;

			// Save iterations
			for (int it = 0; it < r_hist.size(); it++) {
				ss << path << iterfolder << img << "_prec" << tvln << "_" << paramstr << "_" << it + 1 << "i.png";
				if (color) { writeColorImage(r_hist [it].X, g_hist [it].X, b_hist [it].X, ss.str()); ss.str(""); } else { writeImage(r_hist [it].X, ss.str()); ss.str(""); }

				// Convolve precorrected iteration
				Image r_prec_conv_i, g_prec_conv_i, b_prec_conv_i;
				r_prec_conv_i = convolve(r_hist [it].X, psf, BC_PERIODIC);
				if (color) {
					g_prec_conv_i = convolve(g_hist [it].X, psf, BC_PERIODIC);
					b_prec_conv_i = convolve(b_hist [it].X, psf, BC_PERIODIC);
				}
				ss << path << iterfolder << img << "_prec" << tvln << "_conv_" << paramstr << "_" << it + 1 << "i.png";
				if (color) { writeColorImage(r_prec_conv_i, g_prec_conv_i, b_prec_conv_i, ss.str()); ss.str(""); } else { writeImage(r_prec_conv_i, ss.str()); ss.str(""); }
			}
		}

		//// Loop through thetas
		//std::vector<double> thetas;
		//thetas.push_back(5e1);
		//thetas.push_back(1e2);
		//thetas.push_back(2e2);
		//thetas.push_back(4e2);
		//thetas.push_back(8e2);
		//for (auto theta = thetas.begin(); theta != thetas.end(); theta++) {
		//	int theta_i = std::distance(thetas.begin(), theta) + 1;
		//	int theta_max = std::distance(thetas.begin(), thetas.end());
		//	std::cout << cnt_i << " / " << cnt_max << "\t" << theta_i << " / " << theta_max << std::endl;

		//	ss << contrast->first << "c_" << contrast->second << "o_" << dpt << "d_" << *theta << "t";
		//	std::string paramstr = ss.str(); ss.str("");

		//	// Precorrect
		//	bool force = false;
		//	Options opts; opts.tv = TVL2; opts.relchg = DBL_EPSILON; opts.maxiter = 150;
		//	std::string tvln = opts.tv == TVL1 ? "L1" : "L2";
		//	Image r_prec, g_prec, b_prec;
		//	std::vector<Iteration> r_hist, g_hist, b_hist;
		//	ss << path << img << "_prec" << tvln << "_" << paramstr <<  ".png";
		//	if ((color && !readColorImage(r_prec, g_prec, b_prec, ss.str())) ||
		//		(!color && !readImage(r_prec, ss.str())) || force) {
		//		r_prec = precorrect(r_contrast, psf, *theta, opts, NULL);
		//		if (color) {
		//			g_prec = precorrect(g_contrast, psf, *theta, opts, NULL);
		//			b_prec = precorrect(b_contrast, psf, *theta, opts, NULL);
		//		}
		//		if (color) { writeColorImage(r_prec, g_prec, b_prec, ss.str()); }
		//		else { writeImage(r_prec, ss.str()); }
		//	}
		//	ss.str("");

		//	std::cout << "RTV: " << Image::RTV(r, r_prec) << std::endl;

		//	// Convolve precorrected
		//	Image r_prec_conv, g_prec_conv, b_prec_conv;
		//	ss << path << img << "_prec" << tvln << "_conv_" << paramstr << ".png";
		//	if ((color && !readColorImage(r_prec_conv, g_prec_conv, b_prec_conv, ss.str())) ||
		//		(!color && !readImage(r_prec_conv, ss.str())) || force) {
		//		r_prec_conv = convolve(r_prec, psf, BC_PERIODIC);
		//		if (color) {
		//			g_prec_conv = convolve(g_prec, psf, BC_PERIODIC);
		//			b_prec_conv = convolve(b_prec, psf, BC_PERIODIC);
		//		}
		//		if (color) { writeColorImage(r_prec_conv, g_prec_conv, b_prec_conv, ss.str()); }
		//		else { writeImage(r_prec_conv, ss.str()); }
		//	}
		//	ss.str("");

		//	// Save iterations
		//	for (int it = 0; it < r_hist.size(); it++) {
		//		ss << path << iterfolder << img << "_prec" << tvln << "_" << paramstr << "_" << it+1 << "i.png";
		//		if (color) { writeColorImage(r_hist[it].X, g_hist[it].X, b_hist[it].X, ss.str()); ss.str(""); }
		//		else { writeImage(r_hist[it].X, ss.str()); ss.str(""); }

		//		// Convolve precorrected iteration
		//		Image r_prec_conv_i, g_prec_conv_i, b_prec_conv_i;
		//		r_prec_conv_i = convolve(r_hist[it].X, psf, BC_PERIODIC);
		//		if (color) {
		//			g_prec_conv_i = convolve(g_hist[it].X, psf, BC_PERIODIC);
		//			b_prec_conv_i = convolve(b_hist[it].X, psf, BC_PERIODIC);
		//		}
		//		ss << path << iterfolder << img << "_prec" << tvln << "_conv_" << paramstr << "_" << it+1 << "i.png";
		//		if (color) { writeColorImage(r_prec_conv_i, g_prec_conv_i, b_prec_conv_i, ss.str()); ss.str(""); }
		//		else { writeImage(r_prec_conv_i, ss.str()); ss.str(""); }
		//	}
		//}
	}

	log.close();
	ml << "];" << std::endl;
	ml.close();

	return 0;
}