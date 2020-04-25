// Copyright (c) 2020 Michael Koesel and respective contributors
// SPDX-License-Identifier: MIT
// See accompanying LICENSE file for detailed information

#include "image_creation.h"
#include "color_wheel_adder.h"
#include "dbscan.h"
#include "dogm/dogm.h"
#include "dogm/dogm_types.h"

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

static float pignistic_transformation(float free_mass, float occ_mass)
{
    return occ_mass + 0.5f * (1.0f - occ_mass - free_mass);
}

std::vector<Point<dogm::GridCell>> computeCellsWithVelocity(const dogm::DOGM& grid_map, float min_occupancy_threshold,
                                                            float min_velocity_threshold)
{
    std::vector<Point<dogm::GridCell>> cells_with_velocity;
    for (int y = 0; y < grid_map.getGridSize(); y++)
    {
        for (int x = 0; x < grid_map.getGridSize(); x++)
        {
            int index = y * grid_map.getGridSize() + x;

            const dogm::GridCell& cell = grid_map.grid_cell_array[index];
            float occ = pignistic_transformation(cell.free_mass, cell.occ_mass);
            cv::Mat velocity_mean(2, 1, CV_32FC1);
            velocity_mean.at<float>(0) = cell.mean_x_vel;
            velocity_mean.at<float>(1) = cell.mean_y_vel;

            cv::Mat velocity_covar(2, 2, CV_32FC1);
            velocity_covar.at<float>(0, 0) = cell.var_x_vel;
            velocity_covar.at<float>(1, 0) = cell.covar_xy_vel;
            velocity_covar.at<float>(0, 1) = cell.covar_xy_vel;
            velocity_covar.at<float>(1, 1) = cell.var_y_vel;

            cv::Mat velocity_normalized_by_variance = velocity_mean.t() * velocity_covar.inv() * velocity_mean;

            if (occ >= min_occupancy_threshold &&
                velocity_normalized_by_variance.at<float>(0, 0) >= min_velocity_threshold)
            {
                Point<dogm::GridCell> point;
                point.x = x;
                point.y = y;
                point.data = cell;
                point.cluster_id = UNCLASSIFIED;

                cells_with_velocity.push_back(point);
            }
        }
    }

    return cells_with_velocity;
}

cv::Mat compute_measurement_grid_image(const dogm::DOGM& grid_map)
{
    cv::Mat grid_img(grid_map.getGridSize(), grid_map.getGridSize(), CV_8UC3);
    for (int y = 0; y < grid_map.getGridSize(); y++)
    {
        cv::Vec3b* row_ptr = grid_img.ptr<cv::Vec3b>(y);
        for (int x = 0; x < grid_map.getGridSize(); x++)
        {
            int index = y * grid_map.getGridSize() + x;

            const dogm::MeasurementCell& cell = grid_map.meas_cell_array[index];
            float occ = pignistic_transformation(cell.free_mass, cell.occ_mass);
            uchar temp = static_cast<uchar>(occ * 255.0f);

            row_ptr[x] = cv::Vec3b(255 - temp, 255 - temp, 255 - temp);
        }
    }

    return grid_img;
}

cv::Mat compute_raw_measurement_grid_image(const dogm::DOGM& grid_map)
{
    cv::Mat grid_img(grid_map.getGridSize(), grid_map.getGridSize(), CV_8UC3);
    for (int y = 0; y < grid_map.getGridSize(); y++)
    {
        cv::Vec3b* row_ptr = grid_img.ptr<cv::Vec3b>(y);
        for (int x = 0; x < grid_map.getGridSize(); x++)
        {
            int index = y * grid_map.getGridSize() + x;
            const dogm::MeasurementCell& cell = grid_map.meas_cell_array[index];
            int red = static_cast<int>(cell.occ_mass * 255.0f);
            int green = static_cast<int>(cell.free_mass * 255.0f);
            int blue = 255 - red - green;

            row_ptr[x] = cv::Vec3b(blue, green, red);
        }
    }

    return grid_img;
}

cv::Mat compute_raw_polar_measurement_grid_image(const dogm::DOGM& grid_map)
{
    cv::Mat grid_img(grid_map.getGridSize(), 100, CV_8UC3);
    for (int y = 0; y < grid_map.getGridSize(); y++)
    {
        cv::Vec3b* row_ptr = grid_img.ptr<cv::Vec3b>(y);
        for (int x = 0; x < 100; x++)
        {
            int index = y * 100 + x;
            const dogm::MeasurementCell& cell = grid_map.polar_meas_cell_array[index];
            int red = static_cast<int>(cell.occ_mass * 255.0f);
            int green = static_cast<int>(cell.free_mass * 255.0f);
            int blue = 255 - red - green;

            row_ptr[x] = cv::Vec3b(blue, green, red);
        }
    }

    return grid_img;
}

cv::Mat compute_dogm_image(const dogm::DOGM& grid_map, const std::vector<Point<dogm::GridCell>>& cells_with_velocity)
{
    cv::Mat grid_img(grid_map.getGridSize(), grid_map.getGridSize(), CV_8UC3);
    for (int y = 0; y < grid_map.getGridSize(); y++)
    {
        cv::Vec3b* row_ptr = grid_img.ptr<cv::Vec3b>(y);
        for (int x = 0; x < grid_map.getGridSize(); x++)
        {
            int index = y * grid_map.getGridSize() + x;

            const dogm::GridCell& cell = grid_map.grid_cell_array[index];
            float occ = pignistic_transformation(cell.free_mass, cell.occ_mass);
            uchar grayscale_value = 255 - static_cast<uchar>(floor(occ * 255));

            row_ptr[x] = cv::Vec3b(grayscale_value, grayscale_value, grayscale_value);
        }
    }

    for (const auto& cell : cells_with_velocity)
    {
        float angle = fmodf((atan2(cell.data.mean_y_vel, cell.data.mean_x_vel) * (180.0f / M_PI)) + 360, 360);

        // printf("Angle: %f\n", angle);

        // OpenCV hue range is [0, 179], see https://docs.opencv.org/3.2.0/df/d9d/tutorial_py_colorspaces.html
        const auto hue_opencv = static_cast<uint8_t>(angle * 0.5F);
        cv::Mat hsv{1, 1, CV_8UC3, cv::Scalar(hue_opencv, 255, 255)};
        cv::Mat rgb{1, 1, CV_8UC3};
        cv::cvtColor(hsv, rgb, cv::COLOR_HSV2RGB);

        grid_img.ptr<cv::Vec3b>(static_cast<int>(cell.y))[static_cast<int>(cell.x)] = rgb.at<cv::Vec3b>(0, 0);
    }

    addColorWheelToBottomRightCorner(grid_img);

    return grid_img;
}

cv::Mat compute_particles_image(const dogm::DOGM& grid_map)
{
    cv::Mat particles_img(grid_map.getGridSize(), grid_map.getGridSize(), CV_8UC3, cv::Scalar(0, 0, 0));
    for (int i = 0; i < grid_map.particle_count; i++)
    {
        float x = grid_map.particle_array.state[i][0];
        float y = grid_map.particle_array.state[i][1];

        if ((x >= 0 && x < grid_map.getGridSize()) && (y >= 0 && y < grid_map.getGridSize()))
        {
            particles_img.at<cv::Vec3b>(static_cast<int>(y), static_cast<int>(x)) = cv::Vec3b(0, 0, 255);
        }
    }

    return particles_img;
}

void computeAndSaveResultImages(const dogm::DOGM& grid_map,
                                const std::vector<Point<dogm::GridCell>>& cells_with_velocity, const int step,
                                const bool concatenate_images, const bool show_during_execution)
{
    cv::Mat raw_meas_grid_img = compute_raw_measurement_grid_image(grid_map);
    cv::Mat particle_img = compute_particles_image(grid_map);
    cv::Mat dogm_img = compute_dogm_image(grid_map, cells_with_velocity);

    cv::Mat image_to_show{};
    if (concatenate_images)
    {
        cv::hconcat(dogm_img, particle_img, image_to_show);
        cv::hconcat(image_to_show, raw_meas_grid_img, image_to_show);
        cv::imwrite(cv::format("outputs_iter-%d.png", step + 1), image_to_show);
    }
    else
    {
        cv::imwrite(cv::format("raw_grid_iter-%d.png", step + 1), raw_meas_grid_img);
        cv::imwrite(cv::format("particles_iter-%d.png", step + 1), particle_img);
        cv::imwrite(cv::format("dogm_iter-%d.png", step + 1), dogm_img);
        image_to_show = dogm_img;
    }

    if (show_during_execution)
    {
        cv::imshow("dogm", image_to_show);
        cv::waitKey(1);
    }
}