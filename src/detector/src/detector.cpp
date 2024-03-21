// OpenCV
#include <cstdio>
#include <opencv2/core.hpp>
#include <opencv2/core/base.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>

// STD
#include <algorithm>
#include <cmath>
#include <vector>
#include <iostream>

#include "armor.hpp"
#include "detector.hpp"

Detector::Detector(const int & bin_thres, const int & color, const LightParams & l, const ArmorParams & a) : binary_thres(bin_thres), detect_color(color), l(l), a(a) {
    rbdiff_thres = 50;
    color_ratio_thres = 0.4;
}

std::vector<Armor> Detector::detect(const cv::Mat & input) {
    preprocessImage(input, binary_img);

    lights_ = findLights(input, binary_img);
    armors_ = matchLights(lights_);

    if (!armors_.empty()) {
        classifier->extractNumbers(input, armors_);
        classifier->classify(armors_);
    }
    return armors_;
}

void Detector::preprocessImage(const cv::Mat & rgb_img, cv::Mat & binary_img) {
    cv::Mat gray_img, final_img;
    cv::cvtColor(rgb_img, gray_img, cv::COLOR_RGB2GRAY);
    // cv::bilateralFilter(gray_img, final_img, 5, 50, 50);
    cv::threshold(gray_img, binary_img, binary_thres, 255, cv::THRESH_BINARY); //+cv::THRESH_OTSU
}

std::vector<Light> Detector::findLights(const cv::Mat & rbg_img, const cv::Mat & binary_img) {
    using std::vector;
    vector<vector<cv::Point>> contours;
    vector<cv::Vec4i> hierarchy;
    cv::findContours(binary_img, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    vector<Light> lights;

    for (const auto & contour : contours) {
        if (contour.size() < 5) continue;

        auto r_rect = cv::minAreaRect(contour);
        auto light = Light(r_rect);
        if (isLight(light)) {
        auto rect = light.boundingRect();
        if (    // Avoid assertion failed
            0 <= rect.x && 0 <= rect.width && rect.x + rect.width <= rbg_img.cols && 0 <= rect.y &&
            0 <= rect.height && rect.y + rect.height <= rbg_img.rows) {
            
            int sum_r = 0, sum_b = 0, cnt = 0;
            auto roi = rbg_img(rect);
            // Iterate through the ROI
            for (int i = 0; i < roi.rows; i++) {
                for (int j = 0; j < roi.cols; j++) {
                    // if (cv::pointPolygonTest(contour, cv::Point2f(j + rect.x, i + rect.y), false) >= 0) {
                    // if point is inside contour
                    int b = (int)roi.at<cv::Vec3b>(i, j)[2];
                    int r = (int)roi.at<cv::Vec3b>(i, j)[0];
                    
                    int rbdiff = this->detect_color == RED?r-b:b-r;
                    if (rbdiff > rbdiff_thres) {
                        cnt++;
                    }
                    // sum_r += roi.at<cv::Vec3b>(i, j)[0];
                    // sum_b += roi.at<cv::Vec3b>(i, j)[2];
                    // }
                }
            }
            // Sum of red pixels > sum of blue pixels ?
            // light.color = sum_r > sum_b ? RED : BLUE;
            // std::cout << cnt << " " << cv::contourArea(contour) << std::endl;
            if(cnt > color_ratio_thres * r_rect.size.area()) {
                lights.emplace_back(light);
            }
        }
        }
    }

    return lights;
}

bool Detector::isLight(const Light & light) {
    // The ratio of light (short side / long side)
    float ratio = light.width / light.length;
    bool ratio_ok = l.min_ratio < ratio && ratio < l.max_ratio;
    // printf("light: %.2f %.2f\n", ratio, light.tilt_angle);
    bool angle_ok = light.tilt_angle < l.max_angle;

    bool is_light = ratio_ok && angle_ok;
    
    return is_light;
}

std::vector<Armor> Detector::matchLights(const std::vector<Light> & lights) {
    std::vector<Armor> armors;

    // Loop all the pairing of lights
    for (auto light_1 = lights.begin(); light_1 != lights.end(); light_1++) {
        for (auto light_2 = light_1 + 1; light_2 != lights.end(); light_2++) {
        // printf("%d %d %d\n", light_1->color, light_2->color, detect_color);
        if (light_1->color != detect_color || light_2->color != detect_color) {
            continue;
        }

        if (containLight(*light_1, *light_2, lights)) {
            // printf("Contain light\n");
            continue;
        }

        auto type = isArmor(*light_1, *light_2);
        if (type != ArmorType::INVALID) {
            auto armor = Armor(*light_1, *light_2);
            armor.type = type;
            armors.emplace_back(armor);
        }
        }
    }
    // printf ("Size: %d %d\n", lights.size(), armors.size());
    return armors;
}

// Check if there is another light in the boundingRect formed by the 2 lights
bool Detector::containLight(const Light & light_1, const Light & light_2, const std::vector<Light> & lights) {
    auto points = std::vector<cv::Point2f>{light_1.top, light_1.bottom, light_2.top, light_2.bottom};
    auto bounding_rect = cv::boundingRect(points);

    for (const auto & test_light : lights) {
        if (test_light.center == light_1.center || test_light.center == light_2.center) continue;

        if (
        bounding_rect.contains(test_light.top) || bounding_rect.contains(test_light.bottom) ||
        bounding_rect.contains(test_light.center)) {
        return true;
        }
    }

    return false;
}

ArmorType Detector::isArmor(const Light & light_1, const Light & light_2) {
    // Ratio of the length of 2 lights (short side / long side)
    float light_length_ratio = light_1.length < light_2.length ? light_1.length / light_2.length
                                                                : light_2.length / light_1.length;
    bool light_ratio_ok = light_length_ratio > a.min_light_ratio;
    // Distance between the center of 2 lights (unit : light length)
    float avg_light_length = (light_1.length + light_2.length) / 2;
    float center_distance = cv::norm(light_1.center - light_2.center) / avg_light_length;
    bool center_distance_ok = (a.min_small_center_distance <= center_distance && center_distance < a.max_small_center_distance)
                                || (a.min_large_center_distance <= center_distance && center_distance < a.max_large_center_distance);
    // ROS_INFO("light_ratio: %.2f", light_length_ratio);
    // ROS_INFO("center_distance: %.2f", cv::norm(light_1.center - light_2.center));
    // ROS_INFO("avg_light_length: %.2f", avg_light_length);
    // ROS_INFO("Center distance ratio: %.2f", center_distance);
    // Angle of light center connection
    cv::Point2f diff = light_1.center - light_2.center;
    float angle = std::abs(std::atan(diff.y / diff.x)) / CV_PI * 180;
    // printf ("******Angle:  %.2f %.2f", angle, a.max_angle);
    bool angle_ok = angle < a.max_angle;
    // ROS_INFO("Angle: %.2f", angle);

    bool is_armor = light_ratio_ok && center_distance_ok && angle_ok;

    // Judge armor type
    ArmorType type;
    if (is_armor) {
        type = center_distance > a.min_large_center_distance ? ArmorType::LARGE : ArmorType::SMALL;
    } else {
        type = ArmorType::INVALID;
    }

    return type;
}

cv::Mat Detector::getAllNumbersImage() {
    if (armors_.empty()) {
        return cv::Mat(cv::Size(20, 28), CV_8UC1);
    } else {
        std::vector<cv::Mat> number_imgs;
        number_imgs.reserve(armors_.size());
        for (auto & armor : armors_) {
        number_imgs.emplace_back(armor.number_img);
        }
        cv::Mat all_num_img;
        cv::vconcat(number_imgs, all_num_img);
        return all_num_img;
    }
}