/* Copyright 2007-2015 QReal Research Group
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. */

#include "kitBase/robotModel/robotParts/vectorSensor.h"

using namespace kitBase::robotModel::robotParts;

VectorSensor::VectorSensor(const DeviceInfo &info, const PortInfo &port)
	: AbstractSensor(info, port)
{
	connect(this, &VectorSensor::newData,
			this, [this](const QVariant &reading) { mLastValue = reading.value<QVector<int>>(); });
}

QVector<int> VectorSensor::lastData() const
{
	return mLastValue;
}

void VectorSensor::setLastData(const QVector<int> &data)
{
	emit newData(QVariant::fromValue(data));
}
