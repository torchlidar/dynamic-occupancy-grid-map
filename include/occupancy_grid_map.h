#pragma once

#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

struct GridCell
{
	int start_idx;
	int end_idx;
	float new_born_occ_mass;
	float pers_occ_mass;
	float free_mass;
	float occ_mass;
	float mu_A;
	float mu_UA;

	float w_A;
	float w_UA;

	float mean_x_vel;
	float mean_y_vel;
	float var_x_vel;
	float var_y_vel;
	float covar_xy_vel;
};

struct MeasurementCell
{
	float free_mass;
	float occ_mass;
	float likelihood;
	float p_A;
};

struct Particle
{
	int grid_cell_idx;
	float weight;
	bool associated;
	glm::vec4 state;
};

struct GridParams
{
	int width;
	int height;
	float resolution;
	int particle_count;
	int new_born_particle_count;
	float ps;
	float process_noise_position;
	float process_noise_velocity;
	float pb;
};

class OccupancyGridMap
{
public:
	OccupancyGridMap(const GridParams& params);
	~OccupancyGridMap();

	void updateMeasurementGrid(float* measurements);

	void update(float dt, float* measurements);

private:

	void initialize();

public:

	void particlePrediction(float dt);
	void particleAssignment();
	void gridCellOccupancyUpdate();
	void updatePersistentParticles();
	void initializeNewParticles();
	void statisticalMoments();
	void resampling();

private:

	GridParams params;

	GridCell* grid_cell_array;
	Particle* particle_array;
	Particle* particle_array_next;
	Particle* birth_particle_array;

	float* weight_array;
	float* birth_weight_array;
	MeasurementCell* meas_cell_array;
	float* meas_array;

	float* born_masses_array;
	float* particle_orders_array_accum;

	float* vel_x_array;
	float* vel_y_array;

	float* vel_x_squared_array;
	float* vel_y_squared_array;
	float* vel_xy_array;

	float* rand_array;

	int grid_cell_count;
	int particle_count;
	int new_born_particle_count;

	static int BLOCK_SIZE;
};
