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

#pragma once

#include <QtCore/QMap>

#include <qrutils/expressionsParser/expressionsParser.h>
#include <qrgui/mainWindow/errorReporter.h>

namespace qReal {

/// Block diagram action process field parser and interpreter
class BlockParser : public QObject, public utils::ExpressionsParser
{
	Q_OBJECT
public:
	explicit BlockParser(ErrorReporterInterface* errorReporter);

private:

	/// Parse from stream declaration of variables and calcule its values
	virtual void parseVarPart(QString const &stream, int &pos);

	/// Checking for the existence of variable declaration
	virtual void checkForVariable(QString const &nameOfVariable, int &index);

};

}
