#pragma once

#include <AttitudeControlPass.hpp>
#include <AttitudeControlGeom.hpp>
#include <AttitudeControlTilt.hpp>

#include <drivers/drv_hrt.h>

#include <px4_platform_common/module.h>
#include <px4_platform_common/module_params.h>
#include <lib/hysteresis/hysteresis.h>

#include <uORB/Publication.hpp>
#include <uORB/Subscription.hpp>
#include <uORB/SubscriptionCallback.hpp>
#include <uORB/topics/parameter_update.h>

#include <uORB/topics/vehicle_local_position_setpoint.h>
#include <uORB/topics/vehicle_attitude.h>
#include <uORB/topics/vehicle_angular_velocity.h>
#include <uORB/topics/vehicle_thrust_setpoint.h>
#include <uORB/topics/vehicle_torque_setpoint.h>
#include <uORB/topics/vehicle_control_mode.h>
#include <uORB/topics/hover_thrust_estimate.h>
#include <uORB/topics/actuator_controls.h>

// Custom
#include <uORB/topics/ft_sensor.h>
#include <uORB/topics/tilting_servo_sp.h>
/*** END-CUSTOM ***/

// #define TILT_CONTROL
#define GEOM_CONTROL
// #define PASS_CONTROL

#if defined GEOM_CONTROL
	#define POS_OUT_S prisma_geom_pos_out_s
	#define POS_OUT_ORB_ID ORB_ID(prisma_geom_pos_out)
	#define CONTROL_TYPE AttitudeControlGeom
#elif defined PASS_CONTROL
	#define POS_OUT_S prisma_virtual_acc_setpoint_s
	#define POS_OUT_ORB_ID ORB_ID(prisma_virtual_acc_setpoint)
	#define CONTROL_TYPE AttitudeControlPass
#elif defined TILT_CONTROL
	#define POS_OUT_S prisma_tilt_pos_out_s
	#define POS_OUT_ORB_ID ORB_ID(prisma_tilt_pos_out)
	#define CONTROL_TYPE AttitudeControlTilt
#endif

using namespace time_literals;

extern "C" __EXPORT int prisma1_att_control_main(int argc, char *argv[]);

class Prisma1AttitudeControl : public ModuleBase<Prisma1AttitudeControl>, public ModuleParams, public px4::WorkItem
{
public:
	Prisma1AttitudeControl();
	virtual ~Prisma1AttitudeControl() = default;

	/** @see ModuleBase */
	static int task_spawn(int argc, char *argv[]);

	/** @see ModuleBase */
	static int custom_command(int argc, char *argv[]);

	/** @see ModuleBase */
	static int print_usage(const char *reason = nullptr);

	bool init();
	
	// Custom
	void setAdmGains(matrix::Vector3f adm);
	void adm_filter(double dt);
	// End Custom

private:
	bool _is_active;
	long _counter;
	hrt_abstime	_time_stamp_last_loop{0};		/**< time stamp of last loop iteration */

	/** @see ModuleBase::run() */
	void Run() override;
	
	AttitudeControlState set_vehicle_state(const vehicle_attitude_s &att, const vehicle_angular_velocity_s &att_dot);

	void parameters_update(bool force = false);
	uORB::SubscriptionInterval _parameter_update_sub{ORB_ID(parameter_update), 1_s};

	// Input from UAV
	uORB::SubscriptionCallbackWorkItem _vehicle_angular_velocity_sub{this, ORB_ID(vehicle_angular_velocity)};
	uORB::Subscription _vehicle_attitude_sub {ORB_ID(vehicle_attitude)};
	uORB::Subscription _hover_thrust_estimate_sub {ORB_ID(hover_thrust_estimate)};
	uORB::Subscription _vehicle_control_mode_sub {ORB_ID(vehicle_control_mode)};

	// Setpoints from planner
	uORB::Subscription _trajectory_setpoint_sub {ORB_ID(trajectory_setpoint)};
	
	// Custom
	uORB::Subscription _ft_sensor_sub {ORB_ID(ft_sensor)};
    // End Custom

	// Publications
	uORB::Publication<vehicle_thrust_setpoint_s>	_vehicle_thrust_setpoint_pub{ORB_ID(vehicle_thrust_setpoint)};
	uORB::Publication<vehicle_torque_setpoint_s>	_vehicle_torque_setpoint_pub{ORB_ID(vehicle_torque_setpoint)};
	uORB::Publication<actuator_controls_s>		_actuators_0_pub{ORB_ID(actuator_controls_0)};

	CONTROL_TYPE _control;
	vehicle_control_mode_s _vehicle_control_mode {};

	vehicle_attitude_s _attitude;
	vehicle_local_position_setpoint_s _traj_sp;

	// Control inputs
	AttitudeControlInput _setpoint;

    // Custom
	AttitudeControlInput _setpoint_temp;
    ft_sensor_s _ft_fb {};
	matrix::Vector3f _Kp, _Kd, _M;
	matrix::Vector3f _pdd_adm, _pd_adm, _p_adm;
	// End Custom

	/*** CUSTOM ***/
	uORB::Subscription _tilting_servo_sp_sub{ORB_ID(tilting_servo_setpoint)};
	float _tilting_angle_sp{0.0f}; /**< [rad] angle setpoint for tilting servo motors */
	matrix::Vector3f _thrust_setpoint{};
	/*** END-CUSOTM ***/

	systemlib::Hysteresis _failsafe_land_hysteresis{false}; /**< becomes true if task did not update correctly for LOITER_TIME_BEFORE_DESCEND */

	DEFINE_PARAMETERS(
		(ParamFloat<px4::params::PRISMA_THR>) _param_thr,
		(ParamFloat<px4::params::PRISMA_X_TOR>) _param_x_tor,
		(ParamFloat<px4::params::PRISMA_Y_TOR>) _param_y_tor,
		(ParamFloat<px4::params::PRISMA_Z_TOR>) _param_z_tor,
		(ParamFloat<px4::params::PRISMA_IBX>) _param_ibx,
		(ParamFloat<px4::params::PRISMA_IBY>) _param_iby,
		(ParamFloat<px4::params::PRISMA_IBZ>) _param_ibz,
		(ParamFloat<px4::params::PRISMA_MASS>) _param_mass,
		(ParamFloat<px4::params::PRISMA_KR_XY>) _param_xy_kr,
		(ParamFloat<px4::params::PRISMA_KR_Z>) _param_z_kr,
		(ParamFloat<px4::params::PRISMA_KOM_XY>) _param_xy_kom,
		(ParamFloat<px4::params::PRISMA_KOM_Z>) _param_z_kom,
		(ParamFloat<px4::params::PRISMA_KI_ATT_XY>) _param_xy_ki,
		(ParamFloat<px4::params::PRISMA_KI_ATT_Z>) _param_z_ki,
		(ParamFloat<px4::params::PRISMA_C2>) _param_c2,
		(ParamFloat<px4::params::PRISMA_KQ_XY>) _param_xy_kq,
		(ParamFloat<px4::params::PRISMA_KQ_Z>) _param_z_kq,
		(ParamInt<px4::params::PRISMA_ANG_MODE>) _param_angleInputMode,
		// Custom
		(ParamFloat<px4::params::PRISMA_M_ADT>)   _param_m_adm,
		(ParamFloat<px4::params::PRISMA_KD_ADT>)   _param_kd_adm,
		(ParamFloat<px4::params::PRISMA_KP_ADT>)   _param_kp_adm,
		(ParamFloat<px4::params::CA_SV_TL0_MINA>) _param_tilt_min_angle,
		(ParamFloat<px4::params::CA_SV_TL0_MAXA>) _param_tilt_max_angle,
		(ParamInt<px4::params::CA_TILTING_TYPE>)    _param_tilting_type, 	/**< 0:h-tilting, 1:omnidirectional*/
		(ParamInt<px4::params::CA_AIRFRAME>)	    _param_airframe, 		/**< 11: tilting_multirotors */
		(ParamInt<px4::params::MC_PITCH_ON_TILT>)   _param_mpc_pitch_on_tilt
		//End Custom
	);
};
