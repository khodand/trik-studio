#ifndef REALREPOROLES_H
#define REALREPOROLES_H

#include <Qt>

namespace SQLFields {
	static int ElementOffset = 11;

	static const char * Diagram[] = {
		"x", "y", "cfg"
	};

	static const char * Package[] = {
		"foo!"
	};

	static const char * Class[] = {
		"properties", "methods"
	};

	static const char * Link[] = {
		"from", "to"
	};

	static const char ** ColumnNames[] = {
		Diagram,
		Package,
		Class,
		Link
	};

};

namespace UML {
	enum ElementTypes {
		Diagram = 11,
		Package = 12,
		Class = 13,
		Link = 14
	};
};

namespace Unreal {
	enum Roles {
		IdRole = Qt::UserRole + 64,			// per element
		TypeRole,							// same
		PositionRole,						// Pos = (x,y), per diagram
		XRole,
		YRole,
		ConfigurationRole,
		UserRole = Qt::UserRole + 96		// First role available for other types
	};

	enum ClassRoles {
		FieldsRole = UserRole + 1,
		MethodsRole
	};

	enum LinkRoles {
		FromRole = UserRole + 1,
		ToRole,
		FromPortRole,
		ToPortRole
	};
};

#endif
