#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <zbar.h>
#include <filesystem>
//#include "qrcode.h"
#ifdef _WIN32
#include <io.h>
   #define access    _access_s
#else
#include <unistd.h>
#endif

bool FileExists( const std::string &Filename )
{
    return access( Filename.c_str(), 0 ) == 0;
}

using namespace std;
namespace fs = std::filesystem;
using namespace cv;
using namespace zbar;

typedef struct {
    string type;
    string data;
    vector<Point> location;
} decodedObject;

void slidingWindow(cv::Mat LoadedImage);

void decode(Mat &im, vector<decodedObject> &decodedObjects) {
    // Create zbar scanner
    ImageScanner scanner;
    // Configure scanner
    scanner.set_config(ZBAR_NONE, ZBAR_CFG_ENABLE, 1);
    // Convert image to grayscale
    Mat imGray;
    cvtColor(im, imGray, CV_BGR2GRAY);
    // Wrap image data in a zbar image
    Image image(im.cols, im.rows, "Y800", (uchar *) imGray.data, im.cols * im.rows);
    // Scan the image for barcodes and QRCodes
    int n = scanner.scan(image);
    for (Image::SymbolIterator symbol = image.symbol_begin(); symbol != image.symbol_end(); ++symbol) {
        decodedObject obj;
        obj.type = symbol->get_type_name();
        obj.data = symbol->get_data();
        decodedObjects.push_back(obj);
    }
}

Mat binary;
Mat frame;
Mat resized;

int qrcoderead(string filename) {
    VideoCapture cap(filename);
    // Check if camera opened successfully
    if (!cap.isOpened()) {
        cout << "Error opening video stream or file" << endl;
        return -1;
    }
    int64 tduration = cv::getTickCount();       //초기 시작 시간 설정

    double fps = 1 / cap.get(CV_CAP_PROP_FPS);
    cout << "CV_CAP_PROP_FPS : " << fps << endl;
    Mat element = getStructuringElement(MORPH_CROSS,
                                        Size(2 * 1 + 1, 2 * 1 + 1),
                                        Point(1, 1));
    long long frame_count = 1;
    cap >> frame;
//    int ori = 20;
//    double size_d = 0.05;

    int ori = 8;
    double size_d = (double)1/ori;
    while (true) {
        vector<vector<Point> > contours;
        vector<Vec4i> hierarchy;

        cap.grab();
        cap >> frame;
        if (frame.empty())
            break;
        frame_count+=2;

        cv::resize(frame, resized, cv::Size(), size_d, size_d,INTER_NEAREST);
        inRange(resized, Scalar(0, 127, 0), Scalar(100, 255, 100), binary);
        findContours(binary, contours, hierarchy, CV_RETR_CCOMP, CHAIN_APPROX_SIMPLE);

        if(contours.empty())continue;

        vector<Moments> mu(contours.size());
        vector<Point2f> mc(contours.size());
        Scalar color = Scalar(0, 0, 0);
        vector<Point> approx;
        vector<Point2f> dst_vertices = {Point2f(0, 0),
                                        Point2f(0, 400 - 1),
                                        Point2f(400 - 1, 400 - 1),
                                        Point2f(400 - 1, 0)};

        vector<vector<Point2f>> squares;

        if (contours.size() > 1) {
            for (int i = 0; i < contours.size(); i++) {
                approxPolyDP(Mat(contours[i]), approx, 3, true);
                drawContours(resized, contours, i, color, 2, 8, hierarchy, 0, Point());
                Moments mom = moments(approx);
                Point put = Point((mom.m10 / mom.m00), (mom.m01 / mom.m00));
                if (approx.size() == 4) {
                    Point2f left_top;
                    Point2f left_bottom;
                    Point2f right_top;
                    Point2f right_bottom;
                    for (auto i = 0; i < 4; i++) {
                        if (approx[i].x > put.x) {//right
                            if (approx[i].y > put.y) {//bottom
                                right_bottom = approx[i];
                            } else {
                                right_top = approx[i];
                            }
                        } else {//left
                            if (approx[i].y > put.y) {//bottom
                                left_bottom = approx[i];
                            } else {
                                left_top = approx[i];
                            }
                        }
                    }

                    double check1 = sqrt(((left_top.x - left_bottom.x) * (left_top.x - left_bottom.x)) +
                                         ((left_top.y - left_bottom.y) * (left_top.y - left_bottom.y)));
                    double check2 = sqrt(((left_top.x - right_top.x) * (left_top.x - right_top.x)) +
                                         ((left_top.y - right_top.y) * (left_top.y - right_top.y)));
                    if (check1 / check2 < 1.05) {

                        const int mul = ori;

                        vector<Point2f> src_vertices = {
                                Point2f(left_top.x * mul, left_top.y * mul),//left top
                                Point2f(left_bottom.x * mul, left_bottom.y * mul),//left bottom
                                Point2f(right_bottom.x * mul, right_bottom.y * mul),//right bottom
                                Point2f(right_top.x * mul, right_top.y * mul),//right top

                        };//right top
                        squares.push_back(src_vertices);
                    }
                }
            }

            if (squares.size() > 0) {
                vector<decodedObject> decoded;
                for (int j = 0; j < squares.size(); ++j) {

                    Mat out;
                    Mat M = getPerspectiveTransform(squares[j], dst_vertices);
                    cv::warpPerspective(frame, out, M, Size(400, 400));

                    auto out0 = out(Rect(0, 0, 200, 200));
                    auto out1 = out(Rect(0, 200, 200, 200));
                    auto out2 = out(Rect(200, 0, 200, 200));
                    auto out3 = out(Rect(200, 200, 200, 200));
                    decode(out0, decoded);
                    decode(out1, decoded);
                    decode(out2, decoded);
                    decode(out3, decoded);
                }
                std::string::size_type sz = 0;
                long long max = 0;
                if (decoded.size() == 4) {
                    long long ll = std::stoll(decoded[3].data, &sz, 0);
                    max = ll > max ? ll : max;
                } else {
                    if (decoded.size() >= 2) {
                        for (int j = 0; j < decoded.size(); ++j) {
                            long long ll = std::stoll(decoded[j].data, &sz, 0);
                            max = ll > max ? ll : max;
                        }
                    }
                }

                //시간 발견
                if (max != 0) {
                    long long start_time = static_cast<long long int>(max - (frame_count * fps * 1000));
                    long long duration = static_cast<long long int>(cap.get(CV_CAP_PROP_FRAME_COUNT) * fps * 1000);

                    std::cout << "duration! " << duration << std::endl;
                    std::cout << "start_time! " << start_time << std::endl;
                    std::cout << "end_time! " << (start_time + duration) << std::endl;
                    printf("%f ms\n",  (double)(cv::getTickCount() - tduration)/ cv::getTickFrequency());

                    ofstream myfile;
                    myfile.open("result/"+filename + ".txt");
                    myfile << duration << " ";
                    myfile << start_time << " ";
                    myfile << start_time + duration;
                    myfile.close();

                    return 0;
                }
            }
        }
    }
    ofstream myfile;
    myfile.open(filename + ".txt");
    myfile << 0 << " ";
    myfile << 0 << " ";
    myfile << 0;
    myfile.close();
    return 0;
}

int main(int argc, char *argv[]) {
    if(!FileExists("result")){
        fs::path p = string("result");
        fs::create_directory(p);
    }


    string filename;
    if (argc < 2) {
        filename = "../20180618_144311.mp4";
    } else {
        for (int i = 1; i < argc; ++i) {
            qrcoderead(string(argv[i]));
        }
    }
    return 0;
}

