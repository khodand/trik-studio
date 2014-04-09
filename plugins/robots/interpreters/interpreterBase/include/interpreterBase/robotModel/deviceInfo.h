#pragma once

#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>

#include "interpreterBase/interpreterBaseDeclSpec.h"

namespace interpreterBase {
namespace robotModel {

/// Describes a type of a device, not a concrete instance of it. Given a set of DeviceInfo corresponding to
/// some devices hierarchy original inheritance relations can be recovered with isA() method.
class ROBOTS_INTERPRETER_BASE_EXPORT DeviceInfo
{
public:
	/// Represents the direction of communication with this device
	enum Direction
	{
		/// Device provides data
		input = 0
		/// Device accepts data
		, output
	};

	/// Creates a new instance of a Device descriptor. The resulting object will
	/// correspond to a given type only if Q_OBJECT macro is used inside its declaration.
	/// @warning The given device type must contain friendlyName() and direction() static functions
	/// and Q_OBJECT macro.
	template <typename T>
	static DeviceInfo create()
	{
		// This line performs Q_OBJECT macro checking in the given type declaration.
		// Without Q_OBJECT macro incorrect metaObject will be passed and it will lead
		// to invalid isA() method work.
		static_assert(HasQObjectMacro<T>::Value, "No Q_OBJECT macro in the class that is passed into a template");
		QMetaObject const *metaObject = &T::staticMetaObject;
		QString const friendlyName = property(metaObject, "friendlyName");
		Direction const direction = property(metaObject, "direction").toLower() == "input" ? input : output;
		DeviceInfo result(metaObject, friendlyName, direction);
		mCreatedInfos[QString(metaObject->className())] = result;
		return result;
	}

	/// Deserializes inner string representation obtained by toString()
	static DeviceInfo fromString(QString const &string);

	/// Constructs invalid DeviceInfo instance
	DeviceInfo();

	/// Serializes given device info into inner string representation
	QString toString() const;

	/// Returns if the device corresponding to 'this' inherits a 'parent' one or they are the devices of the same type.
	bool isA(DeviceInfo const &parent) const;

	/// Template shorthand notation for inheritance check.
	template<typename T>
	bool isA() const
	{
		return isA(create<T>());
	}

	/// Returns a string that can be displayed to a user as the name of the device.
	QString friendlyName() const;

	/// Returns the direction of communication with devices of this type
	Direction direction() const;

	/// Returns true if device is empty (instantiated with DeviceInfo() constructor).
	bool isNull() const;

private:
	/// Trait that tells if the Object has a Q_OBJECT macro.
	template <typename Object>
	struct HasQObjectMacro
	{
		// Following two overloads will return int if qt_metacall function (generated by Q_OBJECT macro)
		// belongs to 'Object' type (and so Q_OBJECT macro is placed there) and char otherwise.
		template <typename T>
		static char returnIntIfHasQObjectMacro(int (T::*)(QMetaObject::Call, int, void **));
		static int returnIntIfHasQObjectMacro(int (Object::*)(QMetaObject::Call, int, void **));

		// Enum is used for forcing compiler evaluating below expression on compile-time
		enum { Value = sizeof(returnIntIfHasQObjectMacro(&Object::qt_metacall)) == sizeof(int) };
	};

	friend bool operator ==(DeviceInfo const &device1, DeviceInfo const &device2);
	friend bool operator !=(DeviceInfo const &device1, DeviceInfo const &device2);

	DeviceInfo(QMetaObject const *deviceType, QString const &friendlyName, Direction direction);

	static QString property(QMetaObject const * const metaObject, QString const &name);

	static QMap<QString, DeviceInfo> mCreatedInfos;

	QMetaObject const *mDeviceType;
	QString mFriendlyName;
	Direction mDirection;
};

inline bool operator ==(DeviceInfo const &device1, DeviceInfo const &device2)
{
	if (!device1.mDeviceType || !device2.mDeviceType) {
		return device1.mDeviceType == device2.mDeviceType;
	}

	return QString(device1.mDeviceType->className()) == QString(device2.mDeviceType->className());
}

inline bool operator !=(DeviceInfo const &device1, DeviceInfo const &device2)
{
	return !(device1 == device2);
}

}
}

Q_DECLARE_METATYPE(interpreterBase::robotModel::DeviceInfo::Direction)
Q_DECLARE_METATYPE(interpreterBase::robotModel::DeviceInfo)