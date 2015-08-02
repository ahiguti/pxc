//
//  DeviceMotion.h
//  MotionTest
//
//  Created by user on 2015/07/30.
//

namespace pxcrt {
    struct io;
};

namespace pxcrt { namespace ext {

struct device_motion
{
    device_motion(io const& iop, double interval);
    ~device_motion();
    void update();
    float value_mat3[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    float value_xyzw[4] = {0, 0, 0, 1};
private:
    device_motion(const device_motion&) = delete;
    device_motion& operator =(const device_motion&) = delete;
    struct device_motion_impl;
    device_motion_impl *impl;
};

}; };
