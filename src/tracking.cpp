#include "tracking.hpp"

#include <iostream>
#include <opencv/cv.hpp>

using std::string;
using std::shared_ptr;
using namespace cv;
using  namespace std;

shared_ptr<Tracker> Tracker::CreateTracker(const string &name) {
  if (name == "median_flow") {
    return std::make_shared<MedianFlowTracker>();
  }
  else
    throw "name != median_flow";
}

bool MedianFlowTracker::Init(const cv::Mat &frame, const cv::Rect &roi) {
    if (!frame.empty()){
        position_ = roi;
        cvtColor(frame, frame_, CV_BGR2GRAY);
    }
    return !frame_.empty();
}

cv::Rect MedianFlowTracker::Track(const cv::Mat &frame) {
    vector<Point2f> corners;
    Mat object = frame_(position_);
    goodFeaturesToTrack(object, corners, 100, 0.01, 5);

    if (corners.empty()) {
        throw "corners is empty!";
    }

    Mat gray_frame;
    cvtColor(frame, gray_frame, CV_BGR2GRAY);
    vector<uchar> status;
    vector<float> err;
    vector<Point2f> next_pts;

    calcOpticalFlowPyrLK(frame_, gray_frame, corners,
                         next_pts  , status , err);

    for (int i = status.size() - 1; i >= 0; i--) {
        if (status[i] == 0) {
            status.erase(status.begin() + i);
            corners.erase(corners.begin() + i);
            next_pts.erase(next_pts.begin() + i);
            err.erase(err.begin() + i);
        }
    }
    status.clear();
    err.clear();

    if (corners.size() < 2)
        throw "no corners";

    vector<Point2f> prev_pts;
    calcOpticalFlowPyrLK(gray_frame, frame_, next_pts, prev_pts,
                         status, err);
    vector<double> norms;
    for (int i = status.size() - 1; i >= 0; i--) {
        if (status[i] == 0) {
            status.erase(status.begin() + i);
            corners.erase(corners.begin() + i);
            next_pts.erase(next_pts.begin() + i);
            err.erase(err.begin() + i);
        }
    }
    for(int i = 0 ; i < corners.size(); i++){
        norms.push_back(norm(next_pts[i] - prev_pts[i]));
    }
    vector<double> copy_norms(norms.size());
    copy(norms.begin(), norms.end(), copy_norms.begin());
    int size_norms = norms.size();
    nth_element(copy_norms.begin(), copy_norms.begin() + size_norms / 2,
                     copy_norms.end());

    double median = copy_norms[size_norms / 2];

    for (int i = norms.size() - 1; i >= 0; i--) {
        if (norms[i] > median) {
            status.erase(status.begin() + i);
            corners.erase(corners.begin() + i);
            next_pts.erase(next_pts.begin() + i);
            prev_pts.erase(prev_pts.begin() + i);
            norms.erase(norms.begin() + i);
        }
    }
    if(corners.size() < 2)
        throw "no corners";

    vector<float> shiftX, shiftY;
    for (int i = 0; i < corners.size(); ++i) {
        shiftX.push_back((next_pts[i].x - corners[i].x));
        shiftY.push_back((next_pts[i].y - corners[i].y));
    }
    nth_element(shiftX.begin(), shiftX.begin() + shiftX.size() / 2,
                     shiftX.end());
    nth_element(shiftX.begin(), shiftX.begin() + shiftX.size() / 2,
                     shiftX.end());
    Point2f median_shift(shiftX[shiftX.size() / 2],
                         shiftY[shiftY.size() / 2]);

    vector<double> scales;

    for (int i = 0; i < corners.size(); i++) {
        for (int j = i + 1; j < corners.size(); j++) {
            double next_norm = norm(next_pts[i]) ;
            double prev_norm = norm(corners[i]);
            scales.push_back(prev_norm / next_norm);
        }
    }

    nth_element(scales.begin(), scales.begin() + scales.size() / 2,
                scales.end());
    double coeff = scales[scales.size() / 2];

    Rect next_position    = position_ + Point(median_shift);
    next_position.width  *= coeff;
    next_position.height *= coeff;

    position_ = next_position;
    frame_    = gray_frame;

    if(next_position.x < 0 && next_position.x + next_position.width <= 0)
        throw  "no corners";
    if(next_position.x < 0 && next_position.x + next_position.width > 0){
        next_position.width += next_position.x;
        next_position.x = 0;
    }
    if (next_position.x >= frame.cols)
        throw "no corners";
    if (next_position.x < frame.cols && next_position.x + next_position.width > frame.cols){
        next_position.width = frame.cols - next_position.x;
    }

    if(next_position.y < 0 && next_position.y + next_position.height <= 0)
        throw  "no corners";
    if(next_position.y < 0 && next_position.y + next_position.height > 0){
        next_position.height += next_position.y;
        next_position.y = 0;
    }
    if (next_position.y >= frame.rows)
        throw "no corners";
    if (next_position.y < frame.rows && next_position.y + next_position.height > frame.rows){
        next_position.height = frame.rows - next_position.y;
    }

    return next_position;
}