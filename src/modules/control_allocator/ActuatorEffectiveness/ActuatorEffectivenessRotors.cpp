/****************************************************************************
 *
 *   Copyright (c) 2020 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file ActuatorEffectivenessRotors.hpp
 *
 * Actuator effectiveness computed from rotors position and orientation
 *
 * @author Julien Lecoeur <julien.lecoeur@gmail.com>
 */

#include "ActuatorEffectivenessRotors.hpp"

#include "ActuatorEffectivenessTilts.hpp"

using namespace matrix;

// ActuatorEffectivenessRotors::ActuatorEffectivenessRotors(ModuleParams *parent, AxisConfiguration axis_config,
// 		bool tilt_support)
// 	: ModuleParams(parent), _axis_config(axis_config), _tilt_support(tilt_support)
/*** CUSTOM ***/
ActuatorEffectivenessRotors::ActuatorEffectivenessRotors(ModuleParams *parent, AxisConfiguration axis_config,
		bool tilt_support, bool tilting_omnidir)
	: ModuleParams(parent), _axis_config(axis_config), _tilt_support(tilt_support), _tilting_omnidir(tilting_omnidir)
/*** END-CUSOTM ***/
{
	for (int i = 0; i < NUM_ROTORS_MAX; ++i) {
		char buffer[17];
		snprintf(buffer, sizeof(buffer), "CA_ROTOR%u_PX", i);
		_param_handles[i].position_x = param_find(buffer);
		snprintf(buffer, sizeof(buffer), "CA_ROTOR%u_PY", i);
		_param_handles[i].position_y = param_find(buffer);
		snprintf(buffer, sizeof(buffer), "CA_ROTOR%u_PZ", i);
		_param_handles[i].position_z = param_find(buffer);

		if (_axis_config == AxisConfiguration::Configurable) {
			snprintf(buffer, sizeof(buffer), "CA_ROTOR%u_AX", i);
			_param_handles[i].axis_x = param_find(buffer);
			snprintf(buffer, sizeof(buffer), "CA_ROTOR%u_AY", i);
			_param_handles[i].axis_y = param_find(buffer);
			snprintf(buffer, sizeof(buffer), "CA_ROTOR%u_AZ", i);
			_param_handles[i].axis_z = param_find(buffer);
		}

		snprintf(buffer, sizeof(buffer), "CA_ROTOR%u_CT", i);
		_param_handles[i].thrust_coef = param_find(buffer);

		snprintf(buffer, sizeof(buffer), "CA_ROTOR%u_KM", i);
		_param_handles[i].moment_ratio = param_find(buffer);

		if (_tilt_support) {
			snprintf(buffer, sizeof(buffer), "CA_ROTOR%u_TILT", i);
			_param_handles[i].tilt_index = param_find(buffer);
		}
	}

	_count_handle = param_find("CA_ROTOR_COUNT");

	updateParams();
}

void ActuatorEffectivenessRotors::updateParams()
{
	ModuleParams::updateParams();

	int32_t count = 0;

	if (param_get(_count_handle, &count) != 0) {
		PX4_ERR("param_get failed");
		return;
	}

	_geometry.num_rotors = count;

	for (int i = 0; i < _geometry.num_rotors; ++i) {
		Vector3f &position = _geometry.rotors[i].position;
		param_get(_param_handles[i].position_x, &position(0));
		param_get(_param_handles[i].position_y, &position(1));
		param_get(_param_handles[i].position_z, &position(2));

		Vector3f &axis = _geometry.rotors[i].axis;

		switch (_axis_config) {
		case AxisConfiguration::Configurable:
			param_get(_param_handles[i].axis_x, &axis(0));
			param_get(_param_handles[i].axis_y, &axis(1));
			param_get(_param_handles[i].axis_z, &axis(2));
			break;

		case AxisConfiguration::FixedForward:
			axis = Vector3f(1.f, 0.f, 0.f);
			break;

		case AxisConfiguration::FixedUpwards:
			axis = Vector3f(0.f, 0.f, -1.f);
			break;
		}

		param_get(_param_handles[i].thrust_coef, &_geometry.rotors[i].thrust_coef);
		param_get(_param_handles[i].moment_ratio, &_geometry.rotors[i].moment_ratio);

		if (_tilt_support) {
			int32_t tilt_param{0};
			param_get(_param_handles[i].tilt_index, &tilt_param);
			_geometry.rotors[i].tilt_index = tilt_param - 1;

		} else {
			_geometry.rotors[i].tilt_index = -1;
		}
	}
}

bool
ActuatorEffectivenessRotors::addActuators(Configuration &configuration)
{
	if (configuration.num_actuators[(int)ActuatorType::SERVOS] > 0) {
		PX4_ERR("Wrong actuator ordering: servos need to be after motors");
		return false;
	}

	/*** CUSTOM ***/
	int num_actuators = 0;
	// PX4_INFO("Tilting type: %i \n", (int)_tilting_omnidir);
	if(_tilting_omnidir){

		if(configuration.selected_matrix == 0){
		//Vertical forces matrix
			num_actuators = computeEffectivenessMatrix(_geometry,
					configuration.effectiveness_matrices[configuration.selected_matrix],
					configuration.num_actuators_matrix[configuration.selected_matrix],
					true, false);
			// PX4_INFO("VERT \n");
			// PX4_INFO("num_actuators: %d \n", num_actuators);
		}
		else if(configuration.selected_matrix == 1){
		//Lateral forces matrix
			num_actuators = computeEffectivenessMatrix(_geometry,
					configuration.effectiveness_matrices[configuration.selected_matrix],
					configuration.num_actuators_matrix[configuration.selected_matrix],
					true, true);
			// PX4_INFO("LAT \n");
			// PX4_INFO("num_actuators: %d \n", num_actuators);
		}

		// PX4_INFO("num_actuators: %d \n", num_actuators);
	}
	else{
		num_actuators = computeEffectivenessMatrix(_geometry,
				configuration.effectiveness_matrices[configuration.selected_matrix],
				configuration.num_actuators_matrix[configuration.selected_matrix]);
	}
	/*** END-CUSTOM ***/

	// int num_actuators = computeEffectivenessMatrix(_geometry,
	// 		    configuration.effectiveness_matrices[configuration.selected_matrix],
	// 		    configuration.num_actuators_matrix[configuration.selected_matrix]);

	configuration.actuatorsAdded(ActuatorType::MOTORS, num_actuators);
	return true;
}

// int
// ActuatorEffectivenessRotors::computeEffectivenessMatrix(const Geometry &geometry,
// 		EffectivenessMatrix &effectiveness, int actuator_start_index)
/*** CUSTOM ***/
int
ActuatorEffectivenessRotors::computeEffectivenessMatrix(const Geometry &geometry,
		EffectivenessMatrix &effectiveness, int actuator_start_index,
		bool tilting_omnidir, bool horizontal_matrix)
/*** END-CUSTOM ***/
{
	int num_actuators = 0;

	for (int i = 0; i < math::min(NUM_ROTORS_MAX, geometry.num_rotors); i++) {

		if (i + actuator_start_index >= NUM_ACTUATORS) {
			break;
		}

		++num_actuators;

		// Get rotor axis
		Vector3f axis = geometry.rotors[i].axis;

		// Normalize axis
		float axis_norm = axis.norm();

		if (axis_norm > FLT_EPSILON) {
			axis /= axis_norm;

		} else {
			// Bad axis definition, ignore this rotor
			continue;
		}

		// Get rotor position
		const Vector3f &position = geometry.rotors[i].position;

		// Get coefficients
		float ct = geometry.rotors[i].thrust_coef;
		float km = geometry.rotors[i].moment_ratio;

		if (geometry.propeller_torque_disabled) {
			km = 0.f;
		}

		if (geometry.propeller_torque_disabled_non_upwards) {
			bool upwards = fabsf(axis(0)) < 0.1f && fabsf(axis(1)) < 0.1f && axis(2) < -0.5f;

			if (!upwards) {
				km = 0.f;
			}
		}

		if (fabsf(ct) < FLT_EPSILON) {
			continue;
		}

		// // Compute thrust generated by this rotor
		// matrix::Vector3f thrust = ct * axis;

		// // Compute moment generated by this rotor
		// matrix::Vector3f moment = ct * position.cross(axis) - ct * km * axis;

		// // Fill corresponding items in effectiveness matrix
		// for (size_t j = 0; j < 3; j++) {
		// 	effectiveness(j, i + actuator_start_index) = moment(j);
		// 	effectiveness(j + 3, i + actuator_start_index) = thrust(j);
		// }

		/*** CUSTOM ***/

		if (!tilting_omnidir){
			// Compute thrust generated by this rotor
			matrix::Vector3f thrust = ct * axis;
			/*** axis is 0, 0, -1 ***/

			// Compute moment generated by this rotor
			matrix::Vector3f moment = ct * position.cross(axis) - ct * km * axis;

			// Fill corresponding items in effectiveness matrix
			for (size_t j = 0; j < 3; j++) {
				effectiveness(j, i + actuator_start_index) = moment(j);
				effectiveness(j + 3, i + actuator_start_index) = thrust(j);
			}

		}else{
			/* Create the omnidirectional tilting matrix
		           (the rotation sign is included in km)
			   for PX4 CCW:1, CW:-1 but that's the opposite of axis,
			   so that's why they use -km .... i think!
			*/

			float rotor_angle = atan2f(position(1),position(0));
			float cos_rotor = cosf(rotor_angle);
			float sin_rotor = sinf(rotor_angle);

			// PX4_INFO("Motor: %d \n", i);
			// PX4_INFO("atan2: %f \n", (double)rotor_angle);
			// PX4_INFO("Pos: %f %f %f \n", (double)position(0), (double)position(1), (double)position(2));
			// PX4_INFO("ct: %f \n", (double)ct);
			// PX4_INFO("km: %f \n", (double)km);
			// PX4_INFO("Horizontal matrix: %d \n", horizontal_matrix ? 1:0);

			// TO DO: check if is better to use ct here or not

			if( !horizontal_matrix ){
				// Mx vertical
				effectiveness(0, i + actuator_start_index) = -ct * position(1);
				// My vertical
				effectiveness(1, i + actuator_start_index) = ct * position(0);
				// Mz vertical
				// TO DO: check if it's - or +
				effectiveness(2, i + actuator_start_index) = km*ct;

				// Thrust vertical
				effectiveness(3, i + actuator_start_index) = 0.f;
				effectiveness(4, i + actuator_start_index) = 0.f;
				// TO DO: check if it's - or +
				effectiveness(5, i + actuator_start_index) = -ct;

			}
			else{

				// Mx horizontal
				effectiveness(0, i + actuator_start_index) = sin_rotor * km*ct;
				// My horizontal
				effectiveness(1, i + actuator_start_index) =  - cos_rotor * km*ct;
				// Mz horizontal
				effectiveness(2, i + actuator_start_index) = ct * sqrt(powf(position(0),2) + powf(position(1),2) ); //(position(0) * cos_rotor + position(1) * sin_rotor);

				// Thrust horizontal
				effectiveness(3, i + actuator_start_index) = -ct * sin_rotor;
				effectiveness(4, i + actuator_start_index) = ct * cos_rotor;
				effectiveness(5, i + actuator_start_index) = 0.f;
			}

		}

		/*** END-CUSTOM ***/
	}

	return num_actuators;
}

uint32_t ActuatorEffectivenessRotors::updateAxisFromTilts(const ActuatorEffectivenessTilts &tilts, float tilt_control)
{
	if (!PX4_ISFINITE(tilt_control)) {
		tilt_control = -1.f;
	}

	uint32_t nontilted_motors = 0;

	for (int i = 0; i < _geometry.num_rotors; ++i) {
		int tilt_index = _geometry.rotors[i].tilt_index;

		if (tilt_index == -1 || tilt_index >= tilts.count()) {
			nontilted_motors |= 1u << i;
			continue;
		}

		const ActuatorEffectivenessTilts::Params &tilt = tilts.config(tilt_index);
		float tilt_angle = math::lerp(tilt.min_angle, tilt.max_angle, (tilt_control + 1.f) / 2.f);
		float tilt_direction = math::radians((float)tilt.tilt_direction);
		_geometry.rotors[i].axis = tiltedAxis(tilt_angle, tilt_direction);
	}

	return nontilted_motors;
}

Vector3f ActuatorEffectivenessRotors::tiltedAxis(float tilt_angle, float tilt_direction)
{
	Vector3f axis{0.f, 0.f, -1.f};
	return Dcmf{Eulerf{0.f, -tilt_angle, tilt_direction}} * axis;
}

uint32_t ActuatorEffectivenessRotors::getUpwardsMotors() const
{
	uint32_t upwards_motors = 0;

	for (int i = 0; i < _geometry.num_rotors; ++i) {
		const Vector3f &axis = _geometry.rotors[i].axis;

		if (fabsf(axis(0)) < 0.1f && fabsf(axis(1)) < 0.1f && axis(2) < -0.5f) {
			upwards_motors |= 1u << i;
		}
	}

	return upwards_motors;
}
