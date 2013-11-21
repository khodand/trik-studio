#include "d2RobotModel.h"
#include "../tracer.h"
#include <qrkernel/settingsManager.h>
#include <qrutils/mathUtils/math.h>
#include <qrutils/mathUtils/geometry.h>
#include <qrutils/mathUtils/gaussNoise.h>

using namespace qReal::interpreters::robots;
using namespace details;
using namespace d2Model;
using namespace mathUtils;

uint const black   = 0xFF000000;
uint const white   = 0xFFFFFFFF;
uint const red     = 0xFFFF0000;
uint const green   = 0xFF008000;
uint const blue    = 0xFF0000FF;
uint const yellow  = 0xFFFFFF00;
uint const cyan    = 0xFF00FFFF;
uint const magenta = 0xFFFF00FF;

unsigned const touchSensorPressedSignal = 1;
unsigned const touchSensorNotPressedSignal = 0;

qreal const spoilColorDispersion = 2.0;
qreal const spoilLightDispersion = 1.0;
qreal const spoilSonarDispersion = 1.5;
qreal const varySpeedDispersion = 0.0125;
qreal const percentSaltPepperNoise = 20.0;

qreal const floorFrictionCoefficient = 0.3;
qreal const wallFrictionCoefficient = 0.2;

D2RobotModel::D2RobotModel(QObject *parent)
	: QObject(parent)
	, mD2ModelWidget(NULL)
	, mEngineA(NULL)
	, mEngineB(NULL)
	, mEngineC(NULL)
	, mDisplay(new NxtDisplay)
	, mTimeline(new Timeline(this))
	, mNoiseGen()
	, mNeedSync(false)
	, mNeedSensorNoise(SettingsManager::value("enableNoiseOfSensors").toBool())
	, mNeedMotorNoise(SettingsManager::value("enableNoiseOfMotors").toBool())
	, mPos(QPointF(0,0))
	, mAngle(0)
	, mInertialMoment(100)
	, mForceMoment(0)
	, mMass(500)
	, mAngularVelocity(0)
{
	mNoiseGen.setApproximationLevel(SettingsManager::value("approximationLevel").toUInt());
	connect(mTimeline, SIGNAL(tick()), this, SLOT(recalculateParams()), Qt::UniqueConnection);
	connect(mTimeline, SIGNAL(nextFrame()), this, SLOT(nextFragment()), Qt::UniqueConnection);
	initPosition();
}

D2RobotModel::~D2RobotModel()
{
}

void D2RobotModel::initPosition()
{
	if (mEngineA) {
		delete mEngineA;
	}
	if (mEngineB) {
		delete mEngineB;
	}
	if (mEngineC) {
		delete mEngineC;
	}

	mEngineA = initEngine(robotWheelDiameterInPx / 2, 0, 0, 0, false);
	mEngineB = initEngine(robotWheelDiameterInPx / 2, 0, 0, 1, false);
	mEngineC = initEngine(robotWheelDiameterInPx / 2, 0, 0, 2, false);
	setBeep(0, 0);
	mPos = mD2ModelWidget ? mD2ModelWidget->robotPos() : QPointF(0, 0);
}

void D2RobotModel::clear()
{
	initPosition();
	mAngle = 0;
	mPos = QPointF(0,0);
}

D2RobotModel::Engine *D2RobotModel::initEngine(int radius, int speed, long unsigned int degrees, int port, bool isUsed)
{
	Engine *engine = new Engine();
	engine->motorFactor = 0;
	engine->radius = radius;
	engine->speed = speed;
	engine->degrees = degrees;
	engine->isUsed = isUsed;
	engine->breakMode = true;
	if (degrees == 0) {
		engine->activeTimeType = DoInf;
	} else {
		engine->activeTimeType = DoByLimit;
	}

	mEngines[port] = engine;
	mTurnoverEngines[port] = 0;
	return engine;
}

void D2RobotModel::setBeep(unsigned freq, unsigned time)
{
	mBeep.freq = freq;
	mBeep.time = time;
}

void D2RobotModel::setNewMotor(int speed, uint degrees, int port, bool breakMode)
{
	mEngines[port]->speed = speed;
	mEngines[port]->degrees = degrees;
	mEngines[port]->isUsed = true;
	mEngines[port]->breakMode = breakMode;
	if (degrees) {
		mEngines[port]->activeTimeType = DoByLimit;
	} else {
		mEngines[port]->activeTimeType = DoInf;
	}

	if (speed) {
		mEngines[port]->motorFactor = 0.5;
	}
}

int D2RobotModel::varySpeed(int const speed) const
{
	qreal const ran = mNoiseGen.generate(mNoiseGen.approximationLevel(), varySpeedDispersion);
	return truncateToInterval(-100, 100, round(speed * (1 + ran)));
}

void D2RobotModel::countMotorTurnover()
{
	foreach (Engine * const motor, mEngines) {
		int const port = mEngines.key(motor);
		qreal const degrees = Timeline::timeInterval * motor->spoiledSpeed * onePercentAngularVelocity;
		mTurnoverEngines[port] += degrees;
		if (motor->isUsed && (motor->activeTimeType == DoByLimit) && (mTurnoverEngines[port] >= motor->degrees)) {
			motor->speed = 0;
			motor->activeTimeType = End;
			emit d2MotorTimeout();
		}
	}
}

int D2RobotModel::readEncoder(int/*inputPort::InputPortEnum*/ const port) const
{
	return mTurnoverEngines[port];
}

void D2RobotModel::resetEncoder(int/*inputPort::InputPortEnum*/ const port)
{
	mTurnoverEngines[port] = 0;
}

SensorsConfiguration &D2RobotModel::configuration()
{
	return mSensorsConfiguration;
}

D2ModelWidget *D2RobotModel::createModelWidget()
{
	mD2ModelWidget = new D2ModelWidget(this, &mWorldModel, mDisplay);
	return mD2ModelWidget;
}

QPair<QPointF, qreal> D2RobotModel::countPositionAndDirection(robots::enums::inputPort::InputPortEnum const port) const
{
	QVector<SensorItem *> items = mD2ModelWidget->sensorItems();
	SensorItem *sensor = items[port];
	QPointF const position = sensor ? sensor->scenePos() : QPointF();
	qreal const direction = sensor ? items[port]->rotation() + mAngle : 0;
	return QPair<QPointF, qreal>(position, direction);
}

int D2RobotModel::readTouchSensor(robots::enums::inputPort::InputPortEnum const port)
{
	if (mSensorsConfiguration.type(port) != robots::enums::sensorType::touchBoolean
			&& mSensorsConfiguration.type(port) != robots::enums::sensorType::touchRaw)
	{
		return touchSensorNotPressedSignal;
	}
	QPair<QPointF, qreal> neededPosDir = countPositionAndDirection(port);
	QPointF sensorPosition(neededPosDir.first);
	qreal const width = sensorWidth / 2.0;
	QRectF const scanningRect = QRectF(
			sensorPosition.x() - width - touchSensorStrokeIncrement / 2.0
			, sensorPosition.y() - width - touchSensorStrokeIncrement / 2.0
			, 2 * width + touchSensorStrokeIncrement
			, 2 * width + touchSensorStrokeIncrement);

	QPainterPath sensorPath;
	sensorPath.addRect(scanningRect);
	bool const res = mWorldModel.checkCollision(sensorPath, touchSensorWallStrokeIncrement);

	return res ? touchSensorPressedSignal : touchSensorNotPressedSignal;
}

int D2RobotModel::readSonarSensor(robots::enums::inputPort::InputPortEnum const port) const
{
	QPair<QPointF, qreal> neededPosDir = countPositionAndDirection(port);
	int const res = mWorldModel.sonarReading(neededPosDir.first, neededPosDir.second);

	return mNeedSensorNoise ? spoilSonarReading(res) : res;
}

int D2RobotModel::spoilSonarReading(int const distance) const
{
	qreal const ran = mNoiseGen.generate(
			mNoiseGen.approximationLevel()
			, spoilSonarDispersion
			);

	return truncateToInterval(0, 255, round(distance + ran));
}

int D2RobotModel::readColorSensor(robots::enums::inputPort::InputPortEnum const port) const
{
	QImage const image = printColorSensor(port);
	QHash<uint, int> countsColor;

	uint const *data = reinterpret_cast<uint const *>(image.bits());
	int const n = image.byteCount() / 4;
	for (int i = 0; i < n; ++i) {
		uint const color = mNeedSensorNoise ? spoilColor(data[i]) : data[i];
		++countsColor[color];
	}

	switch (mSensorsConfiguration.type(port)) {
	case robots::enums::sensorType::colorFull:
		return readColorFullSensor(countsColor);
	case robots::enums::sensorType::colorNone:
		return readColorNoneSensor(countsColor, n);
	case robots::enums::sensorType::colorRed:
		return readSingleColorSensor(red, countsColor, n);
	case robots::enums::sensorType::colorGreen:
		return readSingleColorSensor(green, countsColor, n);
	case robots::enums::sensorType::colorBlue:
		return readSingleColorSensor(blue, countsColor, n);
	default:
		return 0;
	}
}

uint D2RobotModel::spoilColor(uint const color) const
{
	qreal const ran = mNoiseGen.generate(
			mNoiseGen.approximationLevel()
			, spoilColorDispersion
			);

	int r = round(((color >> 16) & 0xFF) + ran);
	int g = round(((color >> 8) & 0xFF) + ran);
	int b = round(((color >> 0) & 0xFF) + ran);
	int const a = (color >> 24) & 0xFF;

	r = truncateToInterval(0, 255, r);
	g = truncateToInterval(0, 255, g);
	b = truncateToInterval(0, 255, b);

	return ((r & 0xFF) << 16) + ((g & 0xFF) << 8) + (b & 0xFF) + ((a & 0xFF) << 24);
}

QImage D2RobotModel::printColorSensor(robots::enums::inputPort::InputPortEnum const port) const
{
	if (mSensorsConfiguration.type(port) == robots::enums::sensorType::unused) {
		return QImage();
	}

	QPair<QPointF, qreal> const neededPosDir = countPositionAndDirection(port);
	QPointF const position = neededPosDir.first;
	qreal const width = sensorWidth / 2.0;
	QRectF const scanningRect = QRectF(position.x() - width, position.y() - width
			, 2 * width, 2 * width);

	QImage image(scanningRect.size().toSize(), QImage::Format_RGB32);
	QPainter painter(&image);

	QBrush brush(Qt::SolidPattern);
	brush.setColor(Qt::white);
	painter.setBrush(brush);
	painter.setPen(QPen(Qt::white));
	painter.drawRect(scanningRect.translated(-scanningRect.topLeft()));

	bool const wasSelected = mD2ModelWidget->sensorItems()[port]->isSelected();
	mD2ModelWidget->setSensorVisible(port, false);
	mD2ModelWidget->scene()->render(&painter, QRectF(), scanningRect);
	mD2ModelWidget->setSensorVisible(port, true);
	mD2ModelWidget->sensorItems()[port]->setSelected(wasSelected);

	return image;
}

int D2RobotModel::readColorFullSensor(QHash<uint, int> const &countsColor) const
{
	if (countsColor.isEmpty()) {
		return 0;
	}

	QList<int> const values = countsColor.values();
	int maxValue = INT_MIN;
	foreach (int value, values) {
		if (value > maxValue) {
			maxValue = value;
		}
	}

	uint const maxColor = countsColor.key(maxValue);
	switch (maxColor) {
	case (black):
		Tracer::debug(tracer::enums::d2Model, "D2RobotModel::readColorFullSensor", "BLACK");
		return 1;
	case (red):
		Tracer::debug(tracer::enums::d2Model, "D2RobotModel::readColorFullSensor", "RED");
		return 5;
	case (green):
		Tracer::debug(tracer::enums::d2Model, "D2RobotModel::readColorFullSensor", "GREEN");
		return 3;
	case (blue) :
		Tracer::debug(tracer::enums::d2Model, "D2RobotModel::readColorFullSensor", "BLUE");
		return 2;
	case (yellow):
		Tracer::debug(tracer::enums::d2Model, "D2RobotModel::readColorFullSensor", "YELLOW");
		return 4;
	case (white):
		Tracer::debug(tracer::enums::d2Model, "D2RobotModel::readColorFullSensor", "WHITE");
		return 6;
	case (cyan):
		Tracer::debug(tracer::enums::d2Model, "D2RobotModel::readColorFullSensor", "CYAN");
		return 7;
	case (magenta):
		Tracer::debug(tracer::enums::d2Model, "D2RobotModel::readColorFullSensor", "MAGENTA");
		return 8;
	default:
		Tracer::debug(tracer::enums::d2Model, "D2RobotModel::readColorFullSensor", "Other Color");
		return 0;
	}
}

int D2RobotModel::readSingleColorSensor(uint color, QHash<uint, int> const &countsColor, int n) const
{
	return (static_cast<double>(countsColor[color]) / static_cast<double>(n)) * 100.0;
}

int D2RobotModel::readColorNoneSensor(QHash<uint, int> const &countsColor, int n) const
{
	double allWhite = static_cast<double>(countsColor[white]);

	QHashIterator<uint, int> i(countsColor);
	while(i.hasNext()) {
		i.next();
		uint const color = i.key();
		if (color != white) {
			int const b = (color >> 0) & 0xFF;
			int const g = (color >> 8) & 0xFF;
			int const r = (color >> 16) & 0xFF;
			qreal const k = qSqrt(static_cast<qreal>(b * b + g * g + r * r)) / 500.0;
			allWhite += static_cast<qreal>(i.value()) * k;
		}
	}

	return (allWhite / static_cast<qreal>(n)) * 100.0;
}

int D2RobotModel::readLightSensor(robots::enums::inputPort::InputPortEnum const port) const
{
	// Must return 1023 on white and 0 on black normalized to percents
	// http://stackoverflow.com/questions/596216/formula-to-determine-brightness-of-rgb-color

	QImage const image = printColorSensor(port);
	if (image.isNull()) {
		return 0;
	}

	uint sum = 0;
	uint const *data = reinterpret_cast<uint const *>(image.bits());
	int const n = image.byteCount() / 4;

	for (int i = 0; i < n; ++i) {
		int const color = mNeedSensorNoise ? spoilLight(data[i]) : data[i];
		int const b = (color >> 0) & 0xFF;
		int const g = (color >> 8) & 0xFF;
		int const r = (color >> 16) & 0xFF;
		// brightness in [0..256]
		int const brightness = 0.2126 * r + 0.7152 * g + 0.0722 * b;

		sum += 4 * brightness; // 4 = max sensor value / max brightness value
	}
	qreal const rawValue = sum / n; // Average by whole region
	return rawValue * 100 / maxLightSensorValur; // Normalizing to percents
}

uint D2RobotModel::spoilLight(uint const color) const
{
	qreal const ran = mNoiseGen.generate(
			mNoiseGen.approximationLevel()
			, spoilLightDispersion
			);

	if (ran > (1.0 - percentSaltPepperNoise / 100.0)) {
		return white;
	} else if (ran < (-1.0 + percentSaltPepperNoise / 100.0)) {
		return black;
	}

	return color;
}

void D2RobotModel::startInit()
{
	initPosition();
	mTimeline->start();
}

void D2RobotModel::startInterpretation()
{
	startInit();
	mD2ModelWidget->startTimelineListening();
}

void D2RobotModel::stopRobot()
{
	mEngineA->speed = 0;
	mEngineA->breakMode = true;
	mEngineB->speed = 0;
	mEngineB->breakMode = true;
	mEngineC->speed = 0;
	mEngineC->breakMode = true;
	mD2ModelWidget->stopTimelineListening();
}

void D2RobotModel::countBeep()
{
	if (mBeep.time > 0) {
		mD2ModelWidget->drawBeep(true);
		mBeep.time -= Timeline::frameLength;
	} else {
		mD2ModelWidget->drawBeep(false);
	}
}

QLineF D2RobotModel::closestWallBorder(WallItem const &wall, QPointF const &point) const
{
	QLineF closestLine;
	qreal minimum = 10e10;
	for (int i = 0; i < 4; ++i) {
		QLineF const currentLine = wall.line(i);
		qreal const currentDistance = Geometry::distance(currentLine, point);
		if (currentDistance < minimum) {
			minimum = currentDistance;
			closestLine = currentLine;
		}
	}

	return closestLine;
}

void D2RobotModel::updateCoord()
{
	qreal const x = mPos.x() + robotWidth / 2;
	qreal const y = mPos.y() + robotHeight / 2;
	QVector2D const directionVector = robotDirectionVector();
	qreal const dx = directionVector.x() * (robotWidth / 2);
	qreal const dy = directionVector.y() * (robotHeight / 2);

	mVertices[0] = QPointF(x + dx + dy, y + dy - dx);
	mVertices[1] = QPointF(x + dx - dy, y + dy + dx);
	mVertices[2] = QPointF(x - dx - dy, y - dy + dx);
	mVertices[3] = QPointF(x - dx + dy, y - dy - dx);

	for (int i = 0; i < 4; ++i) {
		mBorders[i] = QLineF(mVertices[i], mVertices[(i + 1) % 4]);
	}
}

bool D2RobotModel::isRobotWallCollision(WallItem const &wall) const
{
	QPainterPath const boundingRegion = mD2ModelWidget->robotBoundingPolygon(mPos, mAngle);
	return boundingRegion.intersects(wall.path());
}

void D2RobotModel::findCollision(WallItem const &wall)
{
	mInsidePoints.clear();
	QPainterPath const boundingRegion = mD2ModelWidget->robotBoundingPolygon(mPos, mAngle);
	if (!isRobotWallCollision(wall)) {
		for (int i = 0; i < 4; ++i) {
			if (mRobotWalls[i] == &wall) {
				mRobotWalls[i] = NULL;
				mAngularVelocity = 0;
			}
		}

		return;
	}

	for (int i = 0; i < 4; ++i) {
		if (wall.path().contains(mVertices[i])) {
			if (!mRobotWalls[i]) {
				QLineF const border = wallBorderIntersectedByRobot(wall);
				QPointF const normalPoint = Geometry::normalPoint(border, mVertices[i]);
				QVector2D const normalVector(mVertices[i] - normalPoint);
				mVelocity -= Geometry::projection(mVelocity, normalVector);
			}

			setWall(i, &wall);
		} else {
			if (mRobotWalls[i] == &wall) {
				setWall(i, NULL);
			}

			QPointF const currentWallVertex = wall.point(i);
			if (boundingRegion.contains(currentWallVertex)) {
				mVelocity = QVector2D();
				mInsidePoints << currentWallVertex;
				mAngularVelocity = 0;
			} else {
				if (mRobotEdgeWalls[i] == &wall){
					setEdgeWall(i, NULL);
				}
			}
		}
	}
}

void D2RobotModel::countNewForces()
{
	updateCoord();

	Engine *engine1 = mEngineA;
	Engine *engine2 = mEngineB;

	if (mEngineC->isUsed) {
		if (!mEngineA->isUsed) {
			engine1 = mEngineC;
		} else if (!mEngineB->isUsed) {
			engine2 = mEngineC;
		}
	}

	engine1->spoiledSpeed= mNeedMotorNoise ? varySpeed(engine1->speed) : engine1->speed;
	engine2->spoiledSpeed = mNeedMotorNoise ? varySpeed(engine2->speed) : engine2->speed;

	qreal const speed1 = engine1->spoiledSpeed * 2 * M_PI * engine1->radius * onePercentAngularVelocity / 360 * engine1->motorFactor;
	qreal const speed2 = engine2->spoiledSpeed * 2 * M_PI * engine2->radius * onePercentAngularVelocity / 360 * engine2->motorFactor;

	countTractionForceAndItsMoment(speed1, speed2, engine1->breakMode || engine2->breakMode);
	recalculateVelocity();
	applyRotationalFrictionForce();
}

void D2RobotModel::countTractionForceAndItsMoment(qreal speed1, qreal speed2, bool breakMode)
{
	if (Math::eq(speed1, 0) && Math::eq(speed2, 0)) {
		qreal const realFrictionFactor = breakMode
				? 5 // large value for practically immediate stop
				: floorFrictionCoefficient;
		mTractionForce = -realFrictionFactor * mVelocity;
		mForceMoment = 0;
		return;
	}

	QVector2D const direction = robotDirectionVector();
	QPointF const currentRotationCenter = mPos + rotatePoint;

	QVector2D const traction1Force = direction * speed1 + QVector2D(mVertices[0]);
	QVector2D const traction2Force = direction * speed2 + QVector2D(mVertices[1]);
	QVector2D const rotationPointVector(currentRotationCenter);
	// Parallelogram rule
	mTractionForce = (traction1Force + traction2Force - 2 * rotationPointVector) / 1000;
	mTractionForce -= floorFrictionCoefficient * mVelocity;

	qreal const forceMomentA = Geometry::vectorProduct(direction * speed1, QVector2D(mVertices[0] - currentRotationCenter));
	qreal const forceMomentB = Geometry::vectorProduct(direction * speed2, QVector2D(mVertices[1] - currentRotationCenter));
	mForceMoment = -forceMomentA - forceMomentB;

	for (int i = 0; i < 4; ++i) {
		if (mRobotEdgeWalls[i]) {
			for (int j = 0; j < mInsidePoints.length(); j++) {
				QVector2D const reactionForce = -mTractionForce;
				mForceMoment -= Geometry::vectorProduct(reactionForce, QVector2D(mInsidePoints.at(j) - currentRotationCenter));
			}
		}
	}

	for (int i = 0; i < 4; ++i) {
		if (mRobotWalls[i]) {
			QVector2D tmp(mVertices[i] - currentRotationCenter);
			QLineF const border = closestWallBorder(*mRobotWalls[i], mVertices[i]);
			QVector2D const wallDirectionVector = Geometry::directionVector(border.angle());
			QVector2D const reactionForce = -(mTractionForce - Geometry::projection(mTractionForce, wallDirectionVector));
			QVector2D const frictionForce = -wallDirectionVector.normalized() * reactionForce.length() * wallFrictionCoefficient;
			mTractionForce += reactionForce + frictionForce;
			mForceMoment -= Geometry::vectorProduct(frictionForce, tmp);
			mForceMoment -= Geometry::vectorProduct(reactionForce , tmp);
		}
	}
}

void D2RobotModel::recalculateVelocity()
{
	qreal const angularVelocityFrictionFactor = fabs(mAngularVelocity * 1000);

	mVelocity += mTractionForce / mMass * Timeline::timeInterval;
	mAngularVelocity += mForceMoment / mInertialMoment * Timeline::timeInterval;
	qreal const angularFriction = angularVelocityFrictionFactor / mInertialMoment * Timeline::timeInterval;
	qreal const oldAngularVelocity = mAngularVelocity;

	mAngularVelocity -= angularFriction * Math::sign(mAngularVelocity);

	if (oldAngularVelocity * mAngularVelocity <= 0) {
		mAngularVelocity = 0;
	}
}

void D2RobotModel::applyRotationalFrictionForce()
{
	QVector2D const direction = robotDirectionVector();
	QVector2D rotationalFrictionForce(-direction.y(), direction.x());
	rotationalFrictionForce.normalize();

	qreal const sinus = Geometry::vectorProduct(mVelocity.normalized(), rotationalFrictionForce);
	qreal const rotationalFrictionFactor = mVelocity.length() * 1500;
	rotationalFrictionForce *= sinus * rotationalFrictionFactor;

	if (Geometry::scalarProduct(rotationalFrictionForce, mVelocity) > 0) {
		rotationalFrictionForce = -rotationalFrictionForce;
	}

	QVector2D const newVelocity = mVelocity + rotationalFrictionForce / mMass * Timeline::timeInterval;
	qreal const newProjection = Geometry::scalarProduct(newVelocity, rotationalFrictionForce);
	if (newProjection > 0) {
		qreal const oldProjection = -Geometry::scalarProduct(mVelocity, rotationalFrictionForce);
		qreal const incrementFactor = oldProjection / (oldProjection + newProjection);
		mVelocity += rotationalFrictionForce / mMass * Timeline::timeInterval * incrementFactor;
	} else {
		mVelocity = newVelocity;
	}
}

QVector2D D2RobotModel::robotDirectionVector() const
{
	return Geometry::directionVector(mAngle);
}

void D2RobotModel::getRobotFromWall(WallItem const &wall, int index)
{
	QPainterPath boundingRegion = mD2ModelWidget->robotBoundingPolygon(mPos, mAngle);
	if (isRobotWallCollision(wall)) {
		if (boundingRegion.intersects(wall.path())){
			QPointF const currentVertex = mVertices[index];
			QLineF const border = closestWallBorder(wall, currentVertex);
			mPos += Geometry::normalPoint(border, currentVertex) - currentVertex;
			updateCoord();
		}
	}
}

void D2RobotModel::getEdgeRobotFromWall(WallItem const &wall, int index)
{
	QPainterPath const boundingRegion = mD2ModelWidget->robotBoundingPolygon(mPos, mAngle);
	if (boundingRegion.intersects(wall.path())) {
		for (int i = 0; i < 4; ++i) {
			QPointF const currentPoint = wall.point(i);
			if (boundingRegion.contains(currentPoint)) {
				mPos += currentPoint - Geometry::normalPoint(mBorders[index], currentPoint);
				updateCoord();
			}
		}
	}
}

QLineF D2RobotModel::wallBorderIntersectedByRobot(WallItem const &wall) const
{
	QPainterPath const boundingRegion = mD2ModelWidget->robotBoundingPolygon(mPos, mAngle);
	for (int i = 0; i < 4; ++i) {
		QLineF const currentBorder = wall.line(i);
		if (Geometry::intersects(currentBorder, boundingRegion)) {
			return currentBorder;
		}
	}

	return QLineF();
}

bool D2RobotModel::isCollision(WallItem const &wall, int i) const
{
	return wall.path().contains(mVertices[i]);
}

bool D2RobotModel::isEdgeCollision(WallItem const &wall, int i) const
{
	return Geometry::intersects(mBorders[i], wall.path());
}

void D2RobotModel::nextStep()
{
	qreal const timeInterval = Timeline::timeInterval;
	mPos += mVelocity.toPointF() * timeInterval;
	mAngle += mAngularVelocity * timeInterval;
	updateCoord();
}

void D2RobotModel::recalculateParams()
{
	// do nothing until robot gets back on the ground
	if (!mD2ModelWidget->isRobotOnTheGround()) {
		mNeedSync = true;
		return;
	}

	synchronizePositions();
	for (int i = 0; i < 4; ++i) {
		setWall(i, NULL);
		setEdgeWall(i, NULL);
	}

	for (int i = 0; i < mWorldModel.wallsCount(); ++i) {
		findCollision(*(mWorldModel.wallAt(i)));
		updateCoord();
	}

	countNewForces();
	getFromWalls();
	nextStep();
	countMotorTurnover();
}

void D2RobotModel::getFromWalls()
{
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < mWorldModel.wallsCount(); ++j) {
			if (isCollision(*(mWorldModel.wallAt(j)), i)) {
				getRobotFromWall(*(mWorldModel.wallAt(j)), i);
			}

			if (isEdgeCollision(*(mWorldModel.wallAt(j)), i)) {
				if (mRobotEdgeWalls[i]) {
					getEdgeRobotFromWall(*(mWorldModel.wallAt(j)), i);
				}
			}
		}
	}
}

void D2RobotModel::setWall(int index, WallItem const *wall)
{
	mRobotWalls[index] = wall;
}

void D2RobotModel::setEdgeWall(int index, WallItem const *wall)
{
	mRobotEdgeWalls[index] = wall;
}

void D2RobotModel::nextFragment()
{
	if (!mD2ModelWidget->isRobotOnTheGround()) {
		return;
	}
	synchronizePositions();
	countBeep();
	mD2ModelWidget->draw(mPos, mAngle);
	mNeedSync = true;
}

void D2RobotModel::synchronizePositions()
{
	if (mNeedSync) {
		mPos = mD2ModelWidget->robotPos();
		mNeedSync = false;
	}
}

void D2RobotModel::showModelWidget()
{
	mD2ModelWidget->init(true);
}

void D2RobotModel::setRotation(qreal angle)
{
	mPos = mD2ModelWidget ? mD2ModelWidget->robotPos() : QPointF(0, 0);
	mAngle = fmod(angle, 360);
	mD2ModelWidget->draw(mPos, mAngle);
}

double D2RobotModel::rotateAngle() const
{
	return mAngle;
}

void D2RobotModel::setSpeedFactor(qreal speedMul)
{
	mSpeedFactor = speedMul;
	mTimeline->setSpeedFactor(speedMul);
}

void D2RobotModel::setRobotPos(QPointF const &newPos)
{
	mPos = newPos;
	mD2ModelWidget->draw(mPos, mAngle);
}

QPointF D2RobotModel::robotPos()
{
	return mPos;
}

void D2RobotModel::serialize(QDomDocument &target)
{
	QDomElement robot = target.createElement("robot");
	robot.setAttribute("position", QString::number(mPos.x()) + ":" + QString::number(mPos.y()));
	robot.setAttribute("direction", mAngle);
	configuration().serialize(robot, target);
	target.firstChildElement("root").appendChild(robot);
}

void D2RobotModel::deserialize(QDomElement const &robotElement)
{
	QString const positionStr = robotElement.attribute("position", "0:0");
	QStringList const splittedStr = positionStr.split(":");
	qreal const x = static_cast<qreal>(splittedStr[0].toDouble());
	qreal const y = static_cast<qreal>(splittedStr[1].toDouble());
	mPos = QPointF(x, y);
	mAngle = robotElement.attribute("direction", "0").toDouble();
	configuration().deserialize(robotElement);
	mNeedSync = false;
	nextFragment();
}

Timeline *D2RobotModel::timeline() const
{
	return mTimeline;
}

details::NxtDisplay *D2RobotModel::display()
{
	return mDisplay;
}

void D2RobotModel::setNoiseSettings()
{
	mNeedSensorNoise = SettingsManager::value("enableNoiseOfSensors").toBool();
	mNeedMotorNoise = SettingsManager::value("enableNoiseOfMotors").toBool();
	mNoiseGen.setApproximationLevel(SettingsManager::value("approximationLevel").toUInt());
}

int D2RobotModel::truncateToInterval(int const a, int const b, int const res) const
{
	return (res >= a && res <= b) ? res : (res < a ? a : b);
}

