#include "touchSensor.h"
#include <QDebug>

unsigned const touchSensorResponseSize = 9;

using namespace ev3KitInterpreter::robotModel::real::parts;
using namespace interpreterBase::robotModel;

TouchSensor::TouchSensor(interpreterBase::robotModel::DeviceInfo const &info
		, interpreterBase::robotModel::PortInfo const &port
		, utils::robotCommunication::RobotCommunicator &robotCommunicator)
	: robotParts::TouchSensor(info, port)
	, mImplementation(robotCommunicator, port, SWITCH, READY_SI)
	, mRobotCommunicator(robotCommunicator)
{
}

void TouchSensor::read()
{
	int const size = 21;
	QByteArray command(size, 0);
	command[0] = size - 2;
	command[1] = size >> 8;
	command[2] = 4;
	command[3] = 0;
	command[4] = DIRECT_COMMAND_REPLY;
	int const globalVariablesCount = 4;
	int const localVariablesCount = 0;
	command[5] = globalVariablesCount & 0xFF;
	command[6] = ((localVariablesCount << 2) | (globalVariablesCount >> 8));
	command[7] = opINPUT_DEVICE;
	command[8] = READY_SI;
	command[9] = (PRIMPAR_LONG | PRIMPAR_CONST | PRIMPAR_1_BYTE);
	command[10] = 0x00; //layer
	command[11] = (PRIMPAR_LONG | PRIMPAR_CONST | PRIMPAR_1_BYTE);
	command[12] = mImplementation.lowLevelPort(); //port[0-3]
	command[13] = (PRIMPAR_LONG | PRIMPAR_CONST | PRIMPAR_1_BYTE);
	command[14] = 0x00; //type (0 = Don’t change type)
	command[15] = (PRIMPAR_LONG | PRIMPAR_CONST | PRIMPAR_1_BYTE);
	command[16] = 0x00; //mode – Device mode [0-7]
	command[17] = (PRIMPAR_LONG | PRIMPAR_CONST | PRIMPAR_1_BYTE);
	command[18] = 1; //Number of return values
	command[19] = 225;
	command[20] = 0;

	QByteArray outputBuf;
	mRobotCommunicator.send(command, touchSensorResponseSize, outputBuf);
	if (outputBuf.data()[8] == 63) {
		emit newData(1);
	} else {
		emit newData(0);
	}
}

