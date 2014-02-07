#include "interpreterBase/robotModel/commonRobotModel.h"

#include <qrkernel/exception/exception.h>

using namespace interpreterBase::robotModel;

CommonRobotModel::CommonRobotModel()
	: mBrick(new robotParts::Brick())
	, mDisplay(new robotParts::Display())
{
}

CommonRobotModel::~CommonRobotModel()
{
	stopRobot();
}

void CommonRobotModel::init()
{
	mConfiguration.lockConfiguring();
	connectToRobot();
	configureKnownDevices();
}

void CommonRobotModel::stopRobot()
{
}

void CommonRobotModel::disconnectFromRobot()
{
}

ConfigurationInterface &CommonRobotModel::mutableConfiguration()
{
	return mConfiguration;
}

ConfigurationInterface const &CommonRobotModel::configuration() const
{
	return mConfiguration;
}

robotParts::Brick &CommonRobotModel::brick()
{
	return *mBrick;
}

robotParts::Display &CommonRobotModel::display()
{
	return *mDisplay;
}

QList<PortInfo> CommonRobotModel::availablePorts() const
{
	return mAllowedConnections.keys();
}

QList<PortInfo> CommonRobotModel::configurablePorts() const
{
	return availablePorts();
}

QList<PluggableDeviceInfo> CommonRobotModel::allowedDevices(PortInfo const &port) const
{
	return mAllowedConnections[port];
}

void CommonRobotModel::configureDevice(PortInfo const &port, PluggableDeviceInfo const &deviceInfo)
{
	mConfiguration.configureDevice(createDevice(port, deviceInfo));
}

void CommonRobotModel::setBrick(robotParts::Brick *brick)
{
	mBrick.reset(brick);
}

void CommonRobotModel::setDisplay(robotParts::Display *display)
{
	mDisplay.reset(display);
}

void CommonRobotModel::addAllowedConnection(PortInfo const &port, QList<PluggableDeviceInfo> const &devices)
{
	mAllowedConnections[port].append(devices);
}

void CommonRobotModel::connectToRobot()
{
	emit connected(true);
}

void CommonRobotModel::configureKnownDevices()
{
}

robotParts::PluggableDevice * CommonRobotModel::createDevice(
		PortInfo const &port, PluggableDeviceInfo const &deviceInfo)
{
	Q_UNUSED(port);
	Q_UNUSED(deviceInfo);

	throw qReal::Exception("Unknown pluggable device");
}
