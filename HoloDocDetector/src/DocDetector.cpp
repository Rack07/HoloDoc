#ifdef _DLL_BUILD
#include "stdafx.h"
#endif
#ifdef _DLL_UWP_BUILD
#include "pch.h"
#endif

#include "DocDetector.hpp"
#include <set>

#include <opencv2/imgproc.hpp>
// Meant to disapear (used for simple document detection just to be able to print)
#include <opencv2/highgui.hpp>

using namespace std;
using namespace cv;

//**********************************
//********** DECLARATIONS **********
//**********************************
//*****************
//***** CONST *****
//*****************
const vector<Scalar> COLORS = {
	Scalar(0, 0, 0), Scalar(125, 125, 125), Scalar(255, 255, 255),	// Noir		Gris		Blanc
	Scalar(255, 0, 0), Scalar(0, 255, 0), Scalar(0, 0, 255),	// Rouge	Vert		Bleu
	Scalar(0, 255, 255), Scalar(255, 0, 255), Scalar(255, 255, 0),	// Cyan		Magenta		Jaune
	Scalar(255, 125, 0), Scalar(0, 255, 125), Scalar(125, 0, 255),	// Orange	Turquoise	Indigo
	Scalar(255, 0, 125), Scalar(125, 255, 0), Scalar(0, 125, 255),	// Fushia	Lime		Azur
	Scalar(125, 0, 0), Scalar(0, 125, 0), Scalar(0, 0, 125)		// Blood	Grass		Deep
};

const int COLOR_RANGE = 25;

const double RATIO_LENGTH_MIN = 0.1,
			 RATIO_LENGTH_MAX = 0.7,
			 RATIO_CENTER_DIST = 0.25,
			 RATIO_SIDE = 0.25;
//****************
//***** Misc *****
/// <summary>Sorts the area.</summary>
/// <param name="a">a.</param>
/// <param name="b">The b.</param>
/// <returns></returns>
static bool sortArea(const vector<Point> &a, const vector<Point> &b);

/// <summary>Sorts the points x.</summary>
/// <param name="a">a.</param>
/// <param name="b">The b.</param>
/// <returns></returns>
static bool sortPointsX(const Point &a, const Point &b);

/// <summary>Sorts the points y.</summary>
/// <param name="a">a.</param>
/// <param name="b">The b.</param>
/// <returns></returns>
static bool sortPointsY(const Point &a, const Point &b);

/// <summary>Ins the range.</summary>
/// <param name="x">The x.</param>
/// <param name="min">The minimum.</param>
/// <param name="max">The maximum.</param>
/// <returns></returns>
static bool inRange(double x, double min, double max);

/// <summary>Gets the color of the range.</summary>
/// <param name="background">The background.</param>
/// <param name="lower">The lower.</param>
/// <param name="higher">The higher.</param>
/// <param name="range">The range.</param>
static void GetRangeColor(const Scalar &background, Scalar &lower, Scalar &higher, int range = COLOR_RANGE);

/// <summary>Gets the center.</summary>
/// <param name="v">The v.</param>
/// <returns></returns>
Point GetCenter(const vector<Point> &v);

/// <summary>(Squared) distance between two points (or points to the origin).</summary>
/// <param name="p">The point.</param>
/// <returns></returns>
int SquaredDist(const Point &p);
int SquaredDist(const Point &p1, const Point &p2);
double Dist(const Point &p);
double Dist(const Point &p1, const Point &p2);
//****************

//**********************
//***** Conversion *****
/// <summary>
/// Unity image to OpenCV Mat.
/// </summary>
/// <param name="image">Unity image.</param>
/// <param name="height">Image height.</param>
/// <param name="width">Image width.</param>
/// <param name="dst">tri-channel 8-bit image.</param>
static int UnityToOpenCVMat(Color32 *image, uint height, uint width, Mat &dst);

/// <summary>
/// Unity image to OpenCV Mat, but here it is the inverse.
/// </summary>
/// <param name="input">tri-channel 8-bit image.</param>
/// <param name="output">Unity image.</param>
static int OpenCVMatToUnity(const Mat &input, byte *output);

/// <summary>
/// Docses to unity.
/// </summary>
/// <param name="docs">Vector of Documents. Each doc is represented by a 8-elements vector \f$(x_1, y_1, x_2, y_2, x_3, y_3, x_4, y_4)\f$,
/// where \f$(x_1,y_1)\f$, \f$(x_2, y_2)\f$, \f$(x_3, y_3)\f$ and \f$(x_4, y_4)\f$ are the corner of each detected Document.</param>
/// <param name="dst">The DST.</param>
/// <param name="nbDocuments">Number of documents.</param>
static int DocsToUnity(vector<Vec8i> &docs, int *dst, uint &nbDocuments);

/// <summary>Unity Color to OpenCV Color.</summary>
/// <param name="in">Unity Color</param>
/// <param name="out">OpenCV Color.</param>
static void Color32ToScalar(const Color32 &in, Scalar &out);

/// <summary>Format Contours to vector of coordonnate for Unity.</summary>
/// <param name="contours">The contours.</param>
/// <param name="docs">The docs.</param>
/// TODO:Combine with the DocsToUnity Function
static void ContoursToDocs(const vector<vector<Point>> &contours, vector<Vec8i> &docs);
//**********************

//****************************
//***** Image Processing *****

/// <summary>Binarisations the specified source.</summary>
/// <param name="src">tri-channel 8-bit input image.</param>
/// <param name="dst">single-channel 8-bit binary image.</param>
/// <param name="background">The background color.</param>
/// <param name="range">The color range.</param>
static void Binarisation(const Mat &src, Mat &dst, const Scalar &background = COLORS[0], int range = COLOR_RANGE);

/// <summary>Find Contours.</summary>
/// <param name="src">single-channel 8-bit binary image.</param>
/// <param name="contours">The contours.</param>
/// <param name="length_min">The length minimum of contours.</param>
/// <param name="length_max">The length maximum of contours.</param>
/// <param name="center_dist">The center dist min between 2 contours.</param>
/// <returns></returns>
static int InitContours(const Mat &src, vector<vector<Point>> &contours,
						double length_min, double length_max, double center_dist);

/// <summary>Extract 4 corners of a contour.</summary>
/// <param name="contour">The contour.</param>
/// <param name="length_min">The length minimum.</param>
/// <param name="length_max">The length maximum.</param>
/// <returns><c>True</c> if good length quad is found, <c>False</c> if not</returns>
static bool Extract4Corners(vector<Point> &contour, double length_min, double length_max);

/// <summary>Verify if the point is on the Quad.</summary>
/// <param name="quad">The quad.</param>
/// <param name="point">The point.</param>
/// <returns><c>True</c> if the poitn is in the quad, <c>False</c> if not</returns>
/// TODO:Verify this property before use it
static bool inQuad(const vector<Point> &quad, const Point &point);
//****************************

//*********************************
//********** DEFINITIONS **********
//*********************************
//****************
//***** Misc *****
static bool sortArea(const vector<Point> &a, const vector<Point> &b) { return contourArea(a) > contourArea(b); }
static bool sortPointsX(const Point &a, const Point &b) { return a.x < b.x; }
static bool sortPointsY(const Point &a, const Point &b) { return a.y > b.y; }

bool inRange(const double x, const double min, const double max)
{
	return min <= x && x <= max;
}

void GetRangeColor(const Scalar &background, Scalar &lower, Scalar &higher, int range)
{
	if (range > 127) {
		range = 127;
	}

	for (int i = 0; i < 3; ++i) {
		lower[i] = background[i] - range;
		higher[i] = background[i] + range;
		if (lower[i] < 0) {
			higher[i] -= lower[i];
			lower[i] = 0;
		}
		if (higher[i] > 255) {
			lower[i] -= higher[i] - 255;
			higher[i] = 255;
		}
	}
}

Point GetCenter(const vector<Point> &v)
{
	const int Nb_points = int(v.size());
	Point Center(0, 0);
	for (const Point &p : v) {
		Center += p;
	}
	Center.x /= Nb_points;
	Center.y /= Nb_points;
	return Center;
}

int SquaredDist(const Point &p) { return p.x * p.x + p.y * p.y; }
int SquaredDist(const Point &p1, const Point &p2) { return SquaredDist(p2 - p1); }
double Dist(const Point &p) { return sqrt(SquaredDist(p)); }
double Dist(const Point &p1, const Point &p2) { return sqrt(SquaredDist(p1, p2)); }
//****************

//**********************
//***** Conversion *****

static int UnityToOpenCVMat(Color32 *image, uint height, uint width, Mat &dst)
{
	dst = Mat(height, width, CV_8UC4, image);
	if (dst.empty()) return EMPTY_MAT;
	cvtColor(dst, dst, CV_RGBA2BGR);
	return NO_ERRORS;
}

static int OpenCVMatToUnity(const Mat &input, byte *output)
{
	if (input.empty()) return EMPTY_MAT;
	Mat tmp;
	cvtColor(input, tmp, CV_BGR2RGB);
	memcpy(output, tmp.data, tmp.rows * tmp.cols * 3);
	return NO_ERRORS;
}

static int DocsToUnity(vector<Vec8i> &docs, int *dst, uint &nbDocuments)
{
	if (docs.empty()) return NO_DOCS;

	uint index = 0;
	for (uint i = 0; i < nbDocuments; i++) {
		Vec8i doc = docs[i];
		for (uint j = 0; j < 8; j++) {
			dst[index++] = doc[j];
		}
	}

	return NO_ERRORS;
}


void Color32ToScalar(const Color32 &in, Scalar &out) { out = Scalar(in.b, in.g, in.r); }

void ContoursToDocs(const vector<vector<Point>> &contours, vector<Vec8i> &docs)
{
	docs.resize(contours.size());
	for (int i = 0; i < int(contours.size()); ++i) {
		for (int j = 0; j < 4; ++j) {
			docs[i][2 * j] = contours[i][j].x;
			docs[i][2 * j + 1] = contours[i][j].y;
		}
	}
}
//**********************

//****************************
//***** Image Processing *****
void Binarisation(const Mat &src, Mat &dst, const Scalar &background, int range)
{
	Scalar Lower, Higher;
	GetRangeColor(background, Lower, Higher, range);
	inRange(src, Lower, Higher, dst);
}

int InitContours(const Mat &src, vector<vector<Point>> &contours,
				 const double length_min, const double length_max, const double center_dist)
{
	//Find Contours
	findContours(src, contours, CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE);
	if (contours.empty()) return NO_DOCS;

	//First Vérification
	auto Begin = contours.begin();
	for (int i = 0; i < int(contours.size()); ++i) {
		const double peri = arcLength(contours[i], true);
		if (contours[i].size() < 4 || !inRange(peri, length_min, length_max)) {
			contours.erase(Begin + i);
			i--;
		}
	}
	if (contours.empty()) return NO_DOCS;

	//Extract Corners
	for (int i = 0; i < int(contours.size()); ++i) {
		if (!Extract4Corners(contours[i], length_min, length_max)) {
			contours.erase(Begin + i);
			i--;
		}
	}
	if (contours.empty()) return NO_DOCS;

	//Exlude insiders contours (and double)
	sort(contours.begin(), contours.end(), sortArea);
	Begin = contours.begin();
	for (int i = 0; i < int(contours.size() - 1); ++i) {
		const double Peri = arcLength(contours[i], true);
		if (length_min <= Peri && Peri <= length_max) {
			const Point Center = GetCenter(contours[i]);
			for (int j = i + 1; j < int(contours.size()); ++j) {
				const Point Center2 = GetCenter(contours[j]);
				//	if (inQuad(out[i], Center2)) //doesn't work
				const int Center_dist = SquaredDist(Center, Center2);
				// Same Center or Center distance smallest than including circle radius (it works because contour is sort by area)
				if (Center_dist < center_dist || Center_dist < SquaredDist(Center, contours[i][0])) {
					contours.erase(Begin + j);
					j--;
				}
			}
		}
	}

	//Shape Verification
	//the opposite side must have same distance (with treshold)
	const double Ratio_min = 1 - RATIO_SIDE, Ratio_max = 1 + RATIO_SIDE;
	Begin = contours.begin();
	for (int i = 0; i < int(contours.size()); ++i) {
		int Sides[4];
		Sides[0] = SquaredDist(contours[i][0], contours[i][1]);
		Sides[1] = SquaredDist(contours[i][2], contours[i][3]);
		Sides[2] = SquaredDist(contours[i][1], contours[i][2]);
		Sides[3] = SquaredDist(contours[i][3], contours[i][0]);
		const double Ratio_1 = 1.0 * Sides[0] / Sides[1],
					 Ratio_2 = 1.0 * Sides[2] / Sides[3];
		if (!inRange(Ratio_1, Ratio_min, Ratio_max) || !inRange(Ratio_2, Ratio_min, Ratio_max)) {
			contours.erase(Begin + i);
			i--;
		}
	}

	return NO_ERRORS;
}

bool Extract4Corners(vector<Point> &contour, const double length_min, const double length_max)
{
	const int Nb_points = int(contour.size());
	if (Nb_points < 4) return false;
	if (Nb_points > 4) {
		int Ids[4] = {0, 0, 0, 0};
		set<int> Corners_id;			//Use set to avoid duplication
		Point Center = GetCenter(contour);

		//***** Get Diagonal (Maximize Distance) ****
		for (int k = 2; k < 4; ++k) {
			int Dist_max = 0;
			int Id_farest = 0;
			for (int i = 0; i < Nb_points; ++i) {
				const int dist = SquaredDist(contour[i], Center);
				if (dist > Dist_max) {
					Dist_max = dist;
					Id_farest = i;
				}
			}
			Corners_id.insert(Id_farest);
			Ids[k] = Id_farest;
			Center = contour[Id_farest];
		}

		//***** Find Other Points (Maximize Area) ****
		double Areas_max[2] = {0.0, 0.0};
		const Point AB = contour[Ids[3]] - contour[Ids[2]];
		for (int i = 0; i < Nb_points; ++i) {
			const Point AC = contour[i] - contour[Ids[2]],
						BC = contour[i] - contour[Ids[3]];
			const int d = AB.x * AC.y - AB.y * AC.x;
			//if (d = 0) C is on Diagonal
			if (d != 0) {
				const int side = d > 0 ? 0 : 1;
				const double Dist_AB = Dist(AB),
							 Dist_AC = Dist(AC),
							 Dist_BC = Dist(BC),
							 peri_2 = (Dist_AB + Dist_AC + Dist_BC) / 2;
				// False area based on Heron's formula without square root (maybe avoid on distance too...)
				const double area = peri_2 * (peri_2 - Dist_AC) * (peri_2 - Dist_BC) * (peri_2 - Dist_AB);
				if (area > Areas_max[side]) {
					Areas_max[side] = area;
					Ids[side] = i;
				}
			}
		}
		Corners_id.insert(Ids[0]);
		Corners_id.insert(Ids[1]);

		if (Corners_id.size() != 4) return false;
		vector<Point> Res;
		Res.reserve(4);
		Res.push_back(contour[Ids[2]]);	// First Point Of Diag
		Res.push_back(contour[Ids[0]]);	// Left Point
		Res.push_back(contour[Ids[3]]);	// Second Point of Diag
		Res.push_back(contour[Ids[1]]);	// Right Point
		const double Peri2 = arcLength(Res, true);
		if (!inRange(Peri2, length_min, length_max)) return false;
		contour = Res;
	}
	return true;
}

bool inQuad(const vector<Point> &quad, const Point &point)
{
	Point V = quad[3] - quad[0];
	int cross[2] = {0, 0};
	for (int i = 0; i < 3; ++i) {
		const int sign = V.x * point.y - V.y * point.x >= 0 ? 1 : 0;
		cross[sign]++;
		V = quad[i] - quad[i + 1];
	}
	return cross[0] == 0 || cross[1] == 0;
}
//****************************

//********************************
//********** Unity Link **********
//********************************
extern "C" int __declspec(dllexport) __stdcall DocumentDetection(Color32 *image, uint width, uint height,
																 Color32 background, uint *outDocumentsCount, int *outDocumentsCorners)
{
	Mat src, Im_binary;
	Scalar Background;
	//Conversion
	int errCode = UnityToOpenCVMat(image, height, width, src);
	if (errCode != NO_ERRORS) return errCode;
	//Binarisation
	Color32ToScalar(background, Background);
	Binarisation(src, Im_binary, Background, COLOR_RANGE);
	//FindContour
	vector<vector<Point>> Contours;
	const int perimeter = 2 * (src.cols + src.rows);
	const double Length_min = RATIO_LENGTH_MIN * perimeter,
				 Length_max = RATIO_LENGTH_MAX * perimeter,
				 Center_dist = RATIO_CENTER_DIST * SquaredDist(Point(src.cols, src.rows));
	errCode = InitContours(Im_binary, Contours, Length_min, Length_max, Center_dist);
	if (errCode != NO_ERRORS) return errCode;
	//Conversion
	vector<Vec8i> Docs;
	ContoursToDocs(Contours, Docs);
	*outDocumentsCount = uint(Docs.size());
	return DocsToUnity(Docs, outDocumentsCorners, *outDocumentsCount);
}

extern "C" int __declspec(dllexport) __stdcall SimpleDocumentDetection(Color32 *image, uint width, uint height,
																	   byte *result, uint maxDocumentsCount, uint *outDocumentsCount, int *outDocumentsCorners)
{
	Mat src, edgeDetect;
	UnityToOpenCVMat(image, height, width, src);
	BinaryEdgeDetector(src, edgeDetect);

	vector<vector<Point>> contours;
	findContours(edgeDetect, contours, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE);

	// Sorting the contour depending on their area
	std::sort(contours.begin(), contours.end(), sortArea);

	int index = 0;
	vector<int> viableContoursIndexes;
	vector<Point> approx;
	vector<Vec8i> docsPoints;
	for (const vector<Point> &contour : contours) {
		const double peri = arcLength(contour, true);
		approxPolyDP(contour, approx, 0.02 * peri, true);
		if (approx.size() == 4) {
			// Sorting points
			std::sort(approx.begin(), approx.end(), sortPointsX);
			std::sort(approx.begin() + 1, approx.end() - 1, sortPointsY);

			Vec8i points = {
				approx.at(0).x, int(height) - approx.at(0).y,
				approx.at(1).x, int(height) - approx.at(1).y,
				approx.at(3).x, int(height) - approx.at(3).y,
				approx.at(2).x, int(height) - approx.at(2).y
			};

			docsPoints.emplace_back(points);

			if (docsPoints.size() == maxDocumentsCount) {
				break;
			}

			viableContoursIndexes.push_back(index);
		}
		index++;
	}

	for (int viableContoursIndex : viableContoursIndexes) {
		drawContours(src, contours, viableContoursIndex, Scalar(0, 255, 0), 2);
	}

	*outDocumentsCount = uint(docsPoints.size());
	OpenCVMatToUnity(src, result);
	return DocsToUnity(docsPoints, outDocumentsCorners, *outDocumentsCount);
}

//******************************
//********** Computes **********
//******************************
int BinaryEdgeDetector(const Mat &src, Mat &dst, const int min_tresh, const int max_tresh, const int aperture)
{
	if (src.empty()) return EMPTY_MAT;
	if (src.type() != CV_8UC1 && src.type() != CV_8UC3) return TYPE_MAT;
	Mat gray;
	if (src.type() == CV_8UC3) {
		// Convert image to gray and blur it
		cvtColor(src, gray, CV_BGR2GRAY);
	} else gray = src.clone();

	blur(gray, gray, Size(3, 3));
	Canny(gray, dst, min_tresh, max_tresh, aperture);
	return NO_ERRORS;
}
