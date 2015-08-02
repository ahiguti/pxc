
#import <CoreMotion/CoreMotion.h>
#import "pgl3d_device_motion.h"

namespace pxcrt { namespace ext {

struct device_motion::device_motion_impl
{
    device_motion_impl(double interval) : manager(nullptr) {
        manager = [[CMMotionManager alloc] init];
        manager.deviceMotionUpdateInterval = interval;
        [manager startDeviceMotionUpdates];
    }
    void get_mat3(float value[9]) {
        CMRotationMatrix rot = manager.deviceMotion.attitude.rotationMatrix;
        value[0] = rot.m11;
        value[1] = rot.m12;
        value[2] = rot.m13;
        value[3] = rot.m21;
        value[4] = rot.m22;
        value[5] = rot.m23;
        value[6] = rot.m31;
        value[7] = rot.m32;
        value[8] = rot.m33;
    }
    void get_quat(float value[4]) {
        CMQuaternion quat = manager.deviceMotion.attitude.quaternion;
        value[0] = quat.x;
        value[1] = quat.y;
        value[2] = quat.z;
        value[3] = quat.w;
    }
    CMMotionManager *manager;
};

device_motion::device_motion(io const& iop, double interval)
: impl(new device_motion_impl(interval))
{
}

device_motion::~device_motion()
{
    delete impl;
}

void device_motion::update()
{
    impl->get_mat3(value_mat3);
    impl->get_quat(value_xyzw);
}

}; };
